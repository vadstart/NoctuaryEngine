#include "nt_anim_system.hpp"

namespace nt
{

void AnimationSystem::update(float dt) {
  for (auto const& entity : entities) {
    if (!nexus->HasComponent<cModel>(entity)) continue;
    if (!nexus->HasComponent<cAnimator>(entity)) continue;

    auto& model = nexus->GetComponent<cModel>(entity);
    auto& animator = nexus->GetComponent<cAnimator>(entity);

    animator.animator->update(*model.mesh, dt);
    model.mesh->updateSkeleton();
  }
}

}
