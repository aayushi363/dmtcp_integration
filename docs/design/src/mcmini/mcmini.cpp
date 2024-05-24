#include <cstdint>
#include <memory>
#include <string>

#include "mcmini/coordinator/coordinator.hpp"
#include "mcmini/coordinator/model_to_system_map.hpp"
#include "mcmini/defines.h"
#include "mcmini/lib/shared_transition.h"
#include "mcmini/mem.h"
#include "mcmini/misc/extensions/unique_ptr.hpp"
#include "mcmini/model/exception.hpp"
#include "mcmini/model/objects/mutex.hpp"
#include "mcmini/model/objects/thread.hpp"
#include "mcmini/model/pending_transitions.hpp"
#include "mcmini/model/program.hpp"
#include "mcmini/model/state.hpp"
#include "mcmini/model/state/detached_state.hpp"
#include "mcmini/model/transition.hpp"
#include "mcmini/model/transition_registry.hpp"
#include "mcmini/model/transitions/mutex/mutex_init.hpp"
#include "mcmini/model/transitions/mutex/mutex_lock.hpp"
#include "mcmini/model/transitions/mutex/mutex_unlock.hpp"
#include "mcmini/model/transitions/thread/thread_create.hpp"
#include "mcmini/model/transitions/thread/thread_exit.hpp"
#include "mcmini/model/transitions/thread/thread_start.hpp"
#include "mcmini/model_checking/algorithm.hpp"
#include "mcmini/model_checking/algorithms/classic_dpor.hpp"
#include "mcmini/real_world/mailbox/runner_mailbox.h"
#include "mcmini/real_world/process/fork_process_source.hpp"
#include "mcmini/signal.hpp"

#define _XOPEN_SOURCE_EXTENDED 1

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <utility>

using namespace extensions;
using namespace model;
using namespace objects;
using namespace real_world;

void display_usage() {
  std::cout << "mcmini [options] <program>" << std::endl;
  std::exit(EXIT_FAILURE);
}

void finished_trace_classic_dpor(const coordinator& c) {
  static uint32_t trace_id = 0;

  std::stringstream ss;
  const auto& program_model = c.get_current_program_model();
  ss << "TRACE " << trace_id << "\n";
  for (const auto& t : program_model.get_trace()) {
    ss << "thread " << t->get_executor() << ": " << t->to_string() << "\n";
  }
  ss << "\nNEXT THREAD OPERATIONS\n";
  for (const auto& tpair : program_model.get_pending_transitions()) {
    ss << "thread " << tpair.first << ": " << tpair.second->to_string() << "\n";
  }
  std::cout << ss.str();
  std::cout.flush();
  trace_id++;
}

model::transition* mutex_init_callback(state::runner_id_t p,
                                       const volatile runner_mailbox& rmb,
                                       model_to_system_map& m) {
  // Fetch the remote object
  pthread_mutex_t* remote_mut;
  memcpy_v(&remote_mut, (volatile void*)rmb.cnts, sizeof(pthread_mutex_t*));

  // Locate the corresponding model of this object
  if (!m.contains(remote_mut))
    m.observe_object(remote_mut, new mutex(mutex::state_type::uninitialized));

  state::objid_t const mut = m.get_model_of(remote_mut);
  return new transitions::mutex_init(p, mut);
}

model::transition* mutex_lock_callback(state::runner_id_t p,
                                       const volatile runner_mailbox& rmb,
                                       model_to_system_map& m) {
  pthread_mutex_t* remote_mut;
  memcpy_v(&remote_mut, (volatile void*)rmb.cnts, sizeof(pthread_mutex_t*));

  // TODO: add code from Gene's PR here
  if (!m.contains(remote_mut))
    throw undefined_behavior_exception(
        "Attempting to lock an uninitialized mutex");

  state::objid_t const mut = m.get_model_of(remote_mut);
  return new transitions::mutex_lock(p, mut);
}

model::transition* mutex_unlock_callback(state::runner_id_t p,
                                         const volatile runner_mailbox& rmb,
                                         model_to_system_map& m) {
  pthread_mutex_t* remote_mut;
  memcpy_v(&remote_mut, (volatile void*)rmb.cnts, sizeof(pthread_mutex_t*));

  // TODO: add code from Gene's PR here
  if (!m.contains(remote_mut))
    throw undefined_behavior_exception(
        "Attempting to lock an uninitialized mutex");

  state::objid_t const mut = m.get_model_of(remote_mut);
  return new transitions::mutex_unlock(p, mut);
}

model::transition* thread_create_callback(state::runner_id_t p,
                                          const volatile runner_mailbox& rmb,
                                          model_to_system_map& m) {
  pthread_t new_thread;
  memcpy_v(&new_thread, static_cast<const volatile void*>(&rmb.cnts),
           sizeof(pthread_t));
  runner_id_t const new_thread_id = m.observe_runner(
      (void*)new_thread, new objects::thread(objects::thread::embryo),
      [](runner_id_t id) { return new transitions::thread_start(id); });
  return new transitions::thread_create(p, new_thread_id);
}

model::transition* thread_exit_callback(state::runner_id_t p,
                                        const volatile runner_mailbox& rmb,
                                        model_to_system_map& m) {
  return new transitions::thread_exit(p);
}

void do_model_checking(
    /* Pass arguments here or rearrange to configure the checker at
    runtime, e.g. to pick an algorithm, set a max depth, etc. */) {
  detached_state state_of_program_at_main;
  pending_transitions initial_first_steps;
  transition_registry tr;

  state::runner_id_t const main_thread_id = state_of_program_at_main.add_runner(
      new objects::thread(objects::thread::state::running));
  initial_first_steps.set_transition(
      new transitions::thread_start(main_thread_id));

  program model_for_program_starting_at_main(state_of_program_at_main,
                                             std::move(initial_first_steps));

  // For "vanilla" model checking where we start at the beginning of the
  // program, a `fork_process_source suffices` (fork() + exec() brings us to the
  // beginning).
  auto process_source = make_unique<fork_process_source>("hello-world");

  tr.register_transition(MUTEX_INIT_TYPE, &mutex_init_callback);
  tr.register_transition(MUTEX_LOCK_TYPE, &mutex_lock_callback);
  tr.register_transition(MUTEX_UNLOCK_TYPE, &mutex_unlock_callback);
  tr.register_transition(THREAD_CREATE_TYPE, &thread_create_callback);
  tr.register_transition(THREAD_EXIT_TYPE, &thread_exit_callback);

  coordinator coordinator(std::move(model_for_program_starting_at_main),
                          std::move(tr), std::move(process_source));

  std::unique_ptr<model_checking::algorithm> classic_dpor_checker =
      make_unique<model_checking::classic_dpor>();

  model_checking::algorithm::callbacks c;
  c.trace_completed = &finished_trace_classic_dpor;

  classic_dpor_checker->verify_using(coordinator, c);
  std::cout << "Model checking completed!" << std::endl;
}

void do_model_checking_from_dmtcp_ckpt_file(std::string file_name) {
  model::detached_state const state_of_program_at_main;
  model::pending_transitions initial_first_steps;  // TODO: Create initializer
                                                   // or else add other methods

  // // TODO: Complete the initialization of the initial state here, i.e. a
  // // single thread "main" that is alive and then running the transition

  {
    // Read that information from the linked list __inside the restarted
    // image__
    // while (! not all information read yet) {}
    // read(...);

    // auto state_of_some_object_in_the_ckpt_image = new mutex();
    // state_of_program_at_main.add_state_for();
  }

  {
    // initial_first_steps
    // Figure out what thread `N` is doing. This probably involves coordination
    // between libmcmini.so, libdmtcp.so, and the `mcmini` process
  }

  model::program model_for_program_starting_at_main(
      std::move(state_of_program_at_main), std::move(initial_first_steps));

  // TODO: With a checkpoint restart, a fork_process_source doesn't suffice.
  // We'll need to create a different process source that can provide the
  // functionality we need to spawn new processes from the checkpoint image.
  auto process_source =
      extensions::make_unique<real_world::fork_process_source>("ls");

  coordinator coordinator(std::move(model_for_program_starting_at_main),
                          model::transition_registry(),
                          std::move(process_source));

  std::unique_ptr<model_checking::algorithm> classic_dpor_checker =
      extensions::make_unique<model_checking::classic_dpor>();

  classic_dpor_checker->verify_using(coordinator);

  std::cerr << "Model checking completed!" << std::endl;
}

int main_cpp(int argc, const char** argv) {
  install_process_wide_signal_handlers();
  do_model_checking();
  return EXIT_SUCCESS;
}

int main(int argc, const char** argv) {
  // try {

  // } catch (const std::exception& e) {
  //   std::cerr << "ERROR: " << e.what() << std::endl;
  //   return EXIT_FAILURE;
  // } catch (...) {
  //   std::cerr << "ERROR: Unknown error occurred" << std::endl;
  //   return EXIT_FAILURE;
  // }
  return main_cpp(argc, argv);
}
