#include "engine/physics/crs_physics_system.hpp"

//a Jolt.h tem que vir antes de qualquer outro header da Jolt
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyFilter.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/ContactListener.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/HeightFieldShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace crsdk {

    //menor medida de uma forma (a Jolt não aceita forma de espessura zero, como o chão chato)
    constexpr float MIN_SIZE = 0.05f;
    //passo da simulação: um frame mais longo vira vários passos desse tamanho
    constexpr float STEP = 1.f / 60.f;
    //frame mais longo que isso (janela arrastada, breakpoint) não vira uma simulação gigante
    constexpr float MAX_FRAME = 0.25f;

    static JPH::Vec3 toJolt(const glm::vec3 &v) { return {v.x, v.y, v.z}; }
    static glm::vec3 toGlm(const JPH::Vec3 &v) { return {v.GetX(), v.GetY(), v.GetZ()}; }
    static JPH::Quat toJolt(const glm::quat &q) { return {q.x, q.y, q.z, q.w}; }
    static glm::quat toGlm(const JPH::Quat &q) { return {q.GetW(), q.GetX(), q.GetY(), q.GetZ()}; }

    //---- camadas: quem pode bater em quem ----
    //parado não bate em parado (chão contra parede: nunca se mexem, não precisa conferir)
    namespace layers {
        constexpr JPH::ObjectLayer STATIC = 0;
        constexpr JPH::ObjectLayer MOVING = 1;
    }
    namespace broadPhaseLayers {
        constexpr JPH::BroadPhaseLayer STATIC{0};
        constexpr JPH::BroadPhaseLayer MOVING{1};
    }

    class BroadPhaseLayers final : public JPH::BroadPhaseLayerInterface {
        public:
            JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
            JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
                return layer == layers::STATIC ? broadPhaseLayers::STATIC : broadPhaseLayers::MOVING;
            }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
            const char *GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
                return layer == broadPhaseLayers::STATIC ? "STATIC" : "MOVING";
            }
#endif
    };

    class ObjectVsBroadPhase final : public JPH::ObjectVsBroadPhaseLayerFilter {
        public:
            bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer broadPhase) const override {
                return layer == layers::MOVING || broadPhase == broadPhaseLayers::MOVING;
            }
    };

    class ObjectPairs final : public JPH::ObjectLayerPairFilter {
        public:
            bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
                return a == layers::MOVING || b == layers::MOVING;
            }
    };

    //anota os corpos apoiados em algo (contato com a normal pra cima): o onGround dos dinâmicos.
    //A Jolt chama isso das threads dela, ao mesmo tempo: por isso o mutex
    class GroundContacts final : public JPH::ContactListener {
        public:
            void OnContactAdded(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &manifold, JPH::ContactSettings &) override {
                record(a, b, manifold);
            }
            void OnContactPersisted(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &manifold, JPH::ContactSettings &) override {
                record(a, b, manifold);
            }

            void clear() { grounded.clear(); }
            bool isGrounded(JPH::BodyID id) const { return grounded.contains(id); }

        private:
            void record(const JPH::Body &a, const JPH::Body &b, const JPH::ContactManifold &manifold) {
                //a normal aponta de a pra b: pra cima = b está em cima de a (e vice-versa)
                const float up = manifold.mWorldSpaceNormal.GetY();
                std::lock_guard lock{mutex};
                if (up > 0.5f) grounded.insert(b.GetID());
                if (up < -0.5f) grounded.insert(a.GetID());
            }

            std::mutex mutex;
            std::unordered_set<JPH::BodyID> grounded;
    };

    //o que o corpo foi criado com: mudou = refaz o corpo (a Jolt não estica forma pronta)
    struct ShapeKey {
        BodyType type;
        ColliderShape shape;
        glm::vec3 halfExtents;
        float radius;
        float height;
        glm::vec3 center;
        float mass;
        glm::vec3 scale;

        bool operator==(const ShapeKey &) const = default;
    };

    static ShapeKey shapeKey(const CrsGameObject &obj) {
        const RigidBodyComponent &b = *obj.rigidBody;
        return {b.type, b.shape, b.halfExtents, b.radius, b.height, b.center, b.mass, obj.transform.scale};
    }

    //a forma da Jolt pro componente, já com a escala do objeto; a origem da forma = a origem do objeto
    static JPH::RefConst<JPH::Shape> makeShape(const RigidBodyComponent &body, glm::vec3 scale) {
        scale = glm::abs(scale);
        glm::vec3 center = body.center * scale;
        JPH::RefConst<JPH::Shape> shape;
        switch (body.shape) {
            case ColliderShape::Sphere: {
                shape = new JPH::SphereShape(std::max(body.radius * std::max({scale.x, scale.y, scale.z}), MIN_SIZE));
                break;
            }
            case ColliderShape::Capsule: {
                //a Jolt mede a cápsula pela metade da parte reta (sem as pontas redondas)
                const float radius = std::max(body.radius * std::max(scale.x, scale.z), MIN_SIZE);
                const float halfStraight = std::max(body.height * 0.5f * scale.y - radius, MIN_SIZE);
                shape = new JPH::CapsuleShape(halfStraight, radius);
                break;
            }
            case ColliderShape::Box: {
                glm::vec3 half = body.halfExtents * scale;
                //chão chato (altura 0): ganha espessura pra baixo, a face de cima fica onde estava
                if (half.y < MIN_SIZE) center.y -= MIN_SIZE - half.y;
                half = glm::max(half, glm::vec3{MIN_SIZE});
                //cantos levemente arredondados (mais estável), nunca mais que a menor medida
                const float convexRadius = std::min(JPH::cDefaultConvexRadius, std::min({half.x, half.y, half.z}) * 0.5f);
                shape = new JPH::BoxShape(toJolt(half), convexRadius);
                break;
            }
        }
        if (center != glm::vec3{0.f}) {
            shape = new JPH::RotatedTranslatedShape(toJolt(center), JPH::Quat::sIdentity(), shape);
        }
        return shape;
    }

    //altura (a partir da origem) abaixo da qual um contato conta como chão pro personagem:
    //o centro da parte redonda de baixo (cápsula, esfera) ou um pouco acima da base (caixa)
    static float supportHeight(const RigidBodyComponent &body, glm::vec3 scale) {
        scale = glm::abs(scale);
        const float centerY = body.center.y * scale.y;
        switch (body.shape) {
            case ColliderShape::Sphere: return centerY;
            case ColliderShape::Capsule: {
                const float radius = body.radius * std::max(scale.x, scale.z);
                return centerY - std::max(body.height * 0.5f * scale.y - radius, 0.f);
            }
            case ColliderShape::Box: break;
        }
        return centerY - body.halfExtents.y * scale.y + MIN_SIZE;
    }

    //a Jolt tem coisas globais (tipos de forma, alocador): liga uma vez só, e fica até o programa fechar
    static void initJolt() {
        static std::once_flag once;
        std::call_once(once, [] {
            JPH::RegisterDefaultAllocator();
            JPH::Factory::sInstance = new JPH::Factory();
            JPH::RegisterTypes();
        });
    }

    static JPH::uint workerThreads() {
        const unsigned cores = std::thread::hardware_concurrency();
        return cores > 1 ? cores - 1 : 1;  //um núcleo fica pro resto do jogo
    }

    struct PhysicsSystem::World {
        //a ordem importa: o mundo da Jolt usa tudo que vem antes dele, e é destruído antes
        BroadPhaseLayers broadPhaseLayers{};
        ObjectVsBroadPhase objectVsBroadPhase{};
        ObjectPairs objectPairs{};
        GroundContacts contacts{};
        JPH::TempAllocatorImpl tempAllocator{16 * 1024 * 1024};
        JPH::JobSystemThreadPool jobs{JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, static_cast<int>(workerThreads())};
        JPH::PhysicsSystem physics{};

        //o que foi escrito no transform da última vez: diferente = alguém moveu o objeto
        struct Body {
            JPH::BodyID id;
            ShapeKey key;
            glm::vec3 translation;
            glm::vec3 rotation;
            glm::vec3 velocity;
        };
        std::unordered_map<CrsGameObject::id_t, Body> bodies{};

        struct Character {
            JPH::Ref<JPH::CharacterVirtual> character;
            ShapeKey key;
            glm::vec3 translation;
        };
        //depois do physics: o personagem tira o corpo de dentro dele do mundo ao ser destruído
        std::unordered_map<CrsGameObject::id_t, Character> characters{};

        //terreno: um corpo estático (heightfield) por pedaço; qualquer mudança refaz todos
        struct Terrain {
            std::vector<JPH::BodyID> ids;
            const TerrainData *data;
            glm::vec3 corner;
            std::vector<uint64_t> versions;
        };
        std::unordered_map<CrsGameObject::id_t, Terrain> terrains{};

        World() {
            physics.Init(10240, 0, 65536, 10240, broadPhaseLayers, objectVsBroadPhase, objectPairs);
            physics.SetContactListener(&contacts);
        }

        ~World() {
            JPH::BodyInterface &bodyInterface = physics.GetBodyInterface();
            for (auto &kv : bodies) {
                bodyInterface.RemoveBody(kv.second.id);
                bodyInterface.DestroyBody(kv.second.id);
            }
            for (auto &kv : terrains) removeTerrain(kv.second);
        }

        static std::vector<uint64_t> tileVersions(const TerrainData &data) {
            std::vector<uint64_t> versions;
            for (int z = 0; z < data.tilesPerSide(); z++) {
                for (int x = 0; x < data.tilesPerSide(); x++) versions.push_back(data.tile(x, z).version);
            }
            return versions;
        }

        //o terreno do objeto ainda é o mesmo que virou corpo (mesmas alturas, mesmo lugar)
        static bool sameTerrain(const CrsGameObject &obj, const Terrain &terrain) {
            const TerrainData &data = *obj.terrain->data;
            return &data == terrain.data && obj.terrain->corner(obj.transform.translation) == terrain.corner
                && tileVersions(data) == terrain.versions;
        }

        void removeTerrain(const Terrain &terrain) {
            JPH::BodyInterface &bodyInterface = physics.GetBodyInterface();
            for (const JPH::BodyID &id : terrain.ids) {
                bodyInterface.RemoveBody(id);
                bodyInterface.DestroyBody(id);
            }
        }

        void addTerrain(const CrsGameObject &obj) {
            const TerrainData &data = *obj.terrain->data;
            Terrain terrain{{}, &data, obj.terrain->corner(obj.transform.translation), tileVersions(data)};
            for (int tileZ = 0; tileZ < data.tilesPerSide(); tileZ++) {
                for (int tileX = 0; tileX < data.tilesPerSide(); tileX++) {
                    //a altura (x, z) do pedaço vira o ponto (x, altura, z) * escala, como na malha
                    JPH::HeightFieldShapeSettings settings{data.tile(tileX, tileZ).heights.data(), JPH::Vec3::sZero(),
                        JPH::Vec3{TERRAIN_CELL_SIZE, 1.f, TERRAIN_CELL_SIZE}, TERRAIN_TILE_SAMPLES};
                    JPH::ShapeSettings::ShapeResult result = settings.Create();
                    if (result.HasError()) continue;
                    const glm::vec3 position = terrain.corner + glm::vec3{tileX * TERRAIN_TILE_SIZE, 0.f, tileZ * TERRAIN_TILE_SIZE};
                    JPH::BodyCreationSettings body{result.Get(), toJolt(position), JPH::Quat::sIdentity(),
                        JPH::EMotionType::Static, layers::STATIC};
                    body.mUserData = obj.getId();
                    const JPH::BodyID id = physics.GetBodyInterface().CreateAndAddBody(body, JPH::EActivation::DontActivate);
                    if (!id.IsInvalid()) terrain.ids.push_back(id);
                }
            }
            terrains[obj.getId()] = std::move(terrain);
        }

        void removeBody(const Body &body) {
            JPH::BodyInterface &bodyInterface = physics.GetBodyInterface();
            bodyInterface.RemoveBody(body.id);
            bodyInterface.DestroyBody(body.id);
        }

        void addBody(CrsGameObject &obj) {
            const RigidBodyComponent &rigidBody = *obj.rigidBody;
            const bool dynamic = rigidBody.type == BodyType::Dynamic;
            const glm::quat rotation = glm::quat_cast(obj.transform.rotationMatrix());

            JPH::BodyCreationSettings settings{
                makeShape(rigidBody, obj.transform.scale),
                toJolt(obj.transform.translation),
                toJolt(rotation),
                dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
                dynamic ? layers::MOVING : layers::STATIC};
            if (dynamic) {
                //a massa é a do componente (a Jolt calcularia pelo volume, como se fosse água)
                settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
                settings.mMassPropertiesOverride.mMass = std::max(rigidBody.mass, 0.01f);
                settings.mLinearVelocity = toJolt(rigidBody.velocity);
            }
            settings.mUserData = obj.getId();

            const JPH::BodyID id = physics.GetBodyInterface().CreateAndAddBody(
                settings, dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
            if (id.IsInvalid()) return;  //mundo cheio (10240 corpos): o objeto fica sem física
            bodies[obj.getId()] = {id, shapeKey(obj), obj.transform.translation, obj.transform.rotation, rigidBody.velocity};
        }

        void addCharacter(CrsGameObject &obj) {
            const RigidBodyComponent &rigidBody = *obj.rigidBody;
            JPH::CharacterVirtualSettings settings{};
            settings.mShape = makeShape(rigidBody, obj.transform.scale);
            //um corpo de verdade junto do personagem: assim o que cai ou rola também bate nele
            settings.mInnerBodyShape = settings.mShape;
            settings.mInnerBodyLayer = layers::MOVING;
            settings.mSupportingVolume = JPH::Plane{JPH::Vec3::sAxisY(), -supportHeight(rigidBody, obj.transform.scale)};
            settings.mMaxSlopeAngle = JPH::DegreesToRadians(50.f);
            settings.mMass = std::max(rigidBody.mass, 0.01f);
            settings.mMaxStrength = 300.f;  //força pra empurrar caixas (newtons)

            JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
                &settings, toJolt(obj.transform.translation), JPH::Quat::sIdentity(), obj.getId(), &physics);
            characters[obj.getId()] = {character, shapeKey(obj), obj.transform.translation};
        }

        void moveCharacter(CrsGameObject &obj, Character &entry, const glm::vec3 &gravity, float dt) {
            JPH::CharacterVirtual &character = *entry.character;
            RigidBodyComponent &rigidBody = *obj.rigidBody;

            //alguém moveu o objeto (editor, script): o personagem vai junto
            if (obj.transform.translation != entry.translation) {
                character.SetPosition(toJolt(obj.transform.translation));
            }

            //a velocidade de andar (x, z) vem do CharacterControllerSystem; a vertical é a gravidade.
            //Apoiado e sem pular: não acumula queda
            glm::vec3 velocity = rigidBody.velocity;
            const bool supported = character.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
            if (supported && velocity.y <= 0.f) velocity.y = 0.f;
            velocity.y += gravity.y * dt;
            character.SetLinearVelocity(toJolt(velocity));

            const glm::vec3 before = toGlm(character.GetPosition());
            //desliza na parede, sobe degrau (até 40 cm) e gruda no chão ao descer rampa
            JPH::CharacterVirtual::ExtendedUpdateSettings update{};
            character.ExtendedUpdate(dt, toJolt(gravity), update,
                physics.GetDefaultBroadPhaseLayerFilter(layers::MOVING),
                physics.GetDefaultLayerFilter(layers::MOVING),
                JPH::IgnoreSingleBodyFilter{character.GetInnerBodyID()},  //não bate no próprio corpo
                {}, tempAllocator);
            const glm::vec3 after = toGlm(character.GetPosition());

            obj.transform.translation = after;
            entry.translation = after;
            rigidBody.onGround = character.GetGroundState() == JPH::CharacterVirtual::EGroundState::OnGround;
            //a velocidade de verdade (andando contra a parede = parado): animação e o virar do visual usam ela
            const glm::vec3 moved = (after - before) / dt;
            rigidBody.velocity.x = moved.x;
            rigidBody.velocity.z = moved.z;
            if (rigidBody.onGround) rigidBody.velocity.y = 0.f;
            else if (velocity.y > 0.f) rigidBody.velocity.y = std::min(velocity.y, moved.y);  //bateu a cabeça: para de subir
            else rigidBody.velocity.y = velocity.y;
        }

        //alguém moveu um corpo (editor, script): leva ele pro lugar novo
        void pushTransform(CrsGameObject &obj, Body &body) {
            if (obj.transform.translation == body.translation && obj.transform.rotation == body.rotation) return;
            const glm::quat rotation = glm::quat_cast(obj.transform.rotationMatrix());
            physics.GetBodyInterface().SetPositionAndRotation(body.id, toJolt(obj.transform.translation), toJolt(rotation),
                obj.rigidBody->type == BodyType::Dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
            body.translation = obj.transform.translation;
            body.rotation = obj.transform.rotation;
        }

        //o script mudou a velocidade (ex: um impulso pra cima): passa pra Jolt
        void pushVelocity(CrsGameObject &obj, Body &body) {
            if (obj.rigidBody->type != BodyType::Dynamic || obj.rigidBody->velocity == body.velocity) return;
            physics.GetBodyInterface().SetLinearVelocity(body.id, toJolt(obj.rigidBody->velocity));
            body.velocity = obj.rigidBody->velocity;
        }

        //depois da simulação: a Jolt devolve onde o corpo dinâmico foi parar
        void pullTransform(CrsGameObject &obj, Body &body) {
            JPH::BodyInterface &bodyInterface = physics.GetBodyInterface();
            obj.transform.translation = toGlm(bodyInterface.GetPosition(body.id));
            obj.transform.setRotationMatrix(glm::mat3_cast(toGlm(bodyInterface.GetRotation(body.id))));
            obj.rigidBody->velocity = toGlm(bodyInterface.GetLinearVelocity(body.id));
            //corpo parado há um tempo "dorme" e a Jolt para de avisar os contatos dele: fica o que era
            if (bodyInterface.IsActive(body.id)) obj.rigidBody->onGround = contacts.isGrounded(body.id);
            body.translation = obj.transform.translation;
            body.rotation = obj.transform.rotation;
            body.velocity = obj.rigidBody->velocity;
        }
    };

    PhysicsSystem::PhysicsSystem() {
        initJolt();
        world = std::make_unique<World>();
    }

    PhysicsSystem::~PhysicsSystem() = default;

    void PhysicsSystem::clear() {
        world.reset();  //o antigo sai antes do novo nascer (não dois pools de threads ao mesmo tempo)
        world = std::make_unique<World>();
    }

    void PhysicsSystem::update(CrsScene &scene, float dt) {
        if (dt <= 0.f) return;
        dt = std::min(dt, MAX_FRAME);
        World &w = *world;
        w.physics.SetGravity(toJolt(gravity));

        //1) some o corpo de quem foi apagado, perdeu o RigidBody, virou/deixou de ser personagem ou mudou de forma
        std::erase_if(w.bodies, [&](auto &kv) {
            CrsGameObject *obj = scene.findGameObjectById(kv.first);
            const bool keep = obj && obj->rigidBody && !obj->character && shapeKey(*obj) == kv.second.key;
            if (!keep) w.removeBody(kv.second);
            return !keep;
        });
        std::erase_if(w.characters, [&](auto &kv) {
            CrsGameObject *obj = scene.findGameObjectById(kv.first);
            return !(obj && obj->rigidBody && obj->character && shapeKey(*obj) == kv.second.key);
        });
        std::erase_if(w.terrains, [&](auto &kv) {
            CrsGameObject *obj = scene.findGameObjectById(kv.first);
            const bool keep = obj && obj->terrain && World::sameTerrain(*obj, kv.second);
            if (!keep) w.removeTerrain(kv.second);
            return !keep;
        });
        //terrenos antes dos personagens: quem nasce em cima dele já tem chão no primeiro frame
        for (auto &kv : scene.getGameObjects()) {
            if (kv.second.terrain && !w.terrains.contains(kv.first)) w.addTerrain(kv.second);
        }

        //2) cria o que é novo, leva pro lugar novo o que alguém moveu, e anda com os personagens
        for (auto &kv : scene.getGameObjects()) {
            CrsGameObject &obj = kv.second;
            if (!obj.rigidBody) continue;
            if (obj.character) {
                if (!w.characters.contains(obj.getId())) w.addCharacter(obj);
                w.moveCharacter(obj, w.characters.at(obj.getId()), gravity, dt);
            } else if (auto found = w.bodies.find(obj.getId()); found != w.bodies.end()) {
                w.pushTransform(obj, found->second);
                w.pushVelocity(obj, found->second);
            } else {
                w.addBody(obj);
            }
        }

        //3) simula: um passo por 1/60 s do frame
        const int steps = std::max(1, static_cast<int>(std::ceil(dt / STEP - 0.001f)));
        w.contacts.clear();
        w.physics.Update(dt, steps, &w.tempAllocator, &w.jobs);

        //4) devolve o resultado dos dinâmicos pros objetos
        for (auto &kv : w.bodies) {
            CrsGameObject *obj = scene.findGameObjectById(kv.first);
            if (obj && obj->rigidBody->type == BodyType::Dynamic) w.pullTransform(*obj, kv.second);
        }
    }

}
