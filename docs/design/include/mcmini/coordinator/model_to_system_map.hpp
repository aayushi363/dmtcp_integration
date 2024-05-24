#pragma once

#include "mcmini/coordinator/coordinator.hpp"
#include "mcmini/forwards.hpp"
#include "mcmini/misc/optional.hpp"
#include "mcmini/model/state.hpp"
#include "mcmini/real_world/remote_address.hpp"

/**
 * @brief A mapping between the remote addresses pointing to the C/C++ structs
 * the objects in McMini's model are emulating.
 *
 * As McMini explores different paths of execution of the target program at
 * runtime, it may discover new visible objects. However, visible objects are
 * only a _representation in the McMini model_ of the actual underlying
 * structs containing the information used to implement the primitive. The
 * underlying process, however, refers to these objects as pointers to the
 * multi-threaded primitives (e.g. a `pthread_mutex_t*` in
 * `pthread_mutex_lock()`). McMini therefore needs to maintain a
 * correspondence between these addresses and the identifiers in McMini's
 * model used to represent those objects to the model checker.
 *
 * @important: Handles are assumed to remain valid _across process source
 * invocations_. In the future, we could support the ability to _remap_
 * process handles dynamically during each new re-execution scheduled by
 * the coordinator to handle aliasing etc. by using the trace as a total
 * ordering on object-creation events. Until we run into this issue, we leave it
 * for future development.
 */
class model_to_system_map final {
 private:
  coordinator &_coordinator;

  /*
   * Prevent external construction (only the coordinator can construct
   * instances of this class)
   */
  model_to_system_map(coordinator &coordinator) : _coordinator(coordinator) {}
  friend coordinator;

 public:
  model_to_system_map() = delete;

  /**
   * @brief Retrieve the object that corresponds to the given remote address, or
   * `model::invalid_objid` if the address is not contained in this mapping.
   */
  model::state::objid_t get_model_of(real_world::remote_address<void>) const;
  bool contains(real_world::remote_address<void> addr) const {
    return get_model_of(addr) != model::invalid_objid;
  }

  using runner_generation_function =
      std::function<const model::transition *(model::state::runner_id_t)>;

  /**
   * @brief Record the presence of a new visible object that is
   * represented with the system id.
   *
   * @param remote_process_visible_object_handle the address containing
   * the data for the new visible object across process handles of the
   * `real_world::process_source` in the coordinator
   */
  model::state::objid_t observe_object(real_world::remote_address<void>,
                                       const model::visible_object_state *);
  model::state::runner_id_t observe_runner(real_world::remote_address<void>,
                                           const model::visible_object_state *,
                                           runner_generation_function f);
};
