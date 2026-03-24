#include "systems/mesh_generator.hpp"
#include "components/bounding_box_component.hpp"
#include "components/mesh_component.hpp"
#include "components/plane_component.hpp"
#include "components/transform_component.hpp"
#include "core/types.hpp"
#include "ecs/entity.hpp"
#include "renderer/mesh.hpp"
#include "renderer/resource_manager.hpp"

void MeshGenerator::Init(World &world) {
  world.query<PlaneComponent>().eachWithEntity(
      [&](EntityID e, PlaneComponent &plane) {
        Mesh mesh;
        Vec3 normal = plane.normal;

        int widthSegments = plane.widthSegments;
        int heightSegments = plane.heightSegments;

        Vec3 N = normalize(plane.normal);
        Vec3 tangent;
        if (fabs(N.x) > 0.999f)
          tangent = normalize(cross(Vec3(0, 1, 0), N));
        else
          tangent = normalize(cross(Vec3(1, 0, 0), N));

        Vec3 bitangent = normalize(cross(N, tangent));

        float xStep = plane.width / widthSegments;
        float zStep = plane.height / heightSegments;

        for (int z = 0; z <= heightSegments; ++z) {
          for (int x = 0; x <= widthSegments; ++x) {
            float posX = -plane.width / 2.0f + x * xStep;
            float posZ = -plane.height / 2.0f + z * zStep;

            Vec3 pos = tangent * posX + bitangent * posZ;

            mesh.vertices.push_back(pos.x);
            mesh.vertices.push_back(pos.y);
            mesh.vertices.push_back(pos.z);

            mesh.vertices.push_back(N.x);
            mesh.vertices.push_back(N.y);
            mesh.vertices.push_back(N.z);

            float u = static_cast<float>(x) / widthSegments;
            float v = static_cast<float>(z) / heightSegments;
            mesh.vertices.push_back(u);
            mesh.vertices.push_back(v);
          }
        }

        int vert = 0;
        int tris = 0;
        for (int z = 0; z < heightSegments; ++z) {
          for (int x = 0; x < widthSegments; ++x) {
            mesh.indices.push_back(vert);
            mesh.indices.push_back(vert + 1);
            mesh.indices.push_back(vert + widthSegments + 1);
            mesh.indices.push_back(vert + 1);
            mesh.indices.push_back(vert + widthSegments + 2);
            mesh.indices.push_back(vert + widthSegments + 1);

            ++vert;
          }
          ++vert;
        }

        ResourceManager::AddMesh(plane.meshName, mesh);
        world.AddComponent(e, MeshComponent{.meshName = plane.meshName});
        world.AddComponent(e, TransformComponent{
                                  .position = Vec3(0.0f, 0.0f, 0.0f),
                                  .rotation = Vec3(0.0f, 0.0f, 0.0f),
                                  .scale = Vec3(1.0f),
                              });
        world.AddComponent(
            e, BoundingBoxComponent{.min = Vec3(0.0f), .max = Vec3(0.0f)});
        world.RemoveComponent<PlaneComponent>(e);
      });
}

void MeshGenerator::Update(float deltaTime) {
  // No dynamic mesh generation needed for now
  return;
}
