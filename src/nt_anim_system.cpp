#include "nt_anim_system.hpp"

namespace nt
{

void AnimationSystem::update(float dt) {
  for (auto const& entity : entities) {
    if (!astral->HasComponent<cModel>(entity)) continue;
    if (!astral->HasComponent<cAnimator>(entity)) continue;

    auto& model = astral->GetComponent<cModel>(entity);
    auto& animator = astral->GetComponent<cAnimator>(entity);

    animator.animator->update(*model.mesh, dt);
    model.mesh->updateSkeleton();
  }
}

}
