#include "mcmini/coordinator/coordinator.hpp"

#include "mcmini/real_world/process.hpp"

using namespace real_world;

coordinator::coordinator(
    model::program &&initial_state,
    model::transition_registry &&runtime_transition_mapping,
    std::unique_ptr<real_world::process_source> &&process_source)
    : current_program_model(std::move(initial_state)),
      runtime_transition_mapping(std::move(runtime_transition_mapping)),
      process_source(std::move(process_source)) {
  this->current_process_handle = this->process_source->force_new_process();
}

void coordinator::execute_runner(process::runner_id_t runner_id) {
  if (!current_process_handle) {
    throw real_world::process::execution_exception(
        "Failed to execute runner with id \"" + std::to_string(runner_id) +
        "\": the process is not alive");
  }
  volatile runner_mailbox *mb =
      this->current_process_handle->execute_runner(runner_id);
  model::transition_registry::runtime_type_id rttid = mb->cnts[0];

  model::transition_registry::transition_discovery_callback callback_function =
      runtime_transition_mapping.get_callback_for(rttid);
  if (!callback_function) {
    throw real_world::process::execution_exception(
        "Execution resulted in a runner scheduled to execute the transition "
        "type with the RTTID '" +
        std::to_string(rttid) +
        "' but this identifier has not been registered before model checking "
        "began. Double check that the coordinator was properly configured "
        "before launch; otherwise, please report this as a bug in "
        "libmcmini.so with this message.");
  }
  model_to_system_map remote_address_mapping = model_to_system_map(*this);
  auto pending_operation = callback_function(*mb, remote_address_mapping);
  if (!pending_operation) {
    throw real_world::process::execution_exception(
        "Failed to translate the data written into the mailbox of runner " +
        std::to_string(runner_id));
  }
  this->current_program_model.model_executing_runner(
      runner_id, std::move(pending_operation));
}

remote_address<void> model_to_system_map::get_remote_process_handle_for_object(
    model::state::objid_t id) const {
  // TODO: Implement his
  return remote_address<void>{};
}

model::state::objid_t model_to_system_map::get_object_for_remote_process_handle(
    remote_address<void> handle) const {
  if (_coordinator.system_address_mapping.count(handle) > 0) {
    return _coordinator.system_address_mapping[handle];
  }
  return model::invalid_objid;
}

model::state::objid_t model_to_system_map::observe_remote_process_handle(
    remote_address<void> remote_process_visible_object_handle,
    std::unique_ptr<model::visible_object_state> fallback_initial_state) {
  model::state::objid_t existing_obj =
      this->get_object_for_remote_process_handle(
          remote_process_visible_object_handle);
  if (existing_obj != model::invalid_objid) {
    return existing_obj;
  }

  // TODO: Inform the coordinator of the new object
  return model::invalid_objid;
}