#include "gvirtus/backend/Process.h"

#include <gvirtus/common/JSON.h>
#include <gvirtus/common/SignalException.h>
#include <gvirtus/common/SignalState.h>

#include <gvirtus/backend/Process.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <functional>
#include <thread>

using gvirtus::backend::Process;
using gvirtus::common::LD_Lib;
using gvirtus::communicators::Buffer;
using gvirtus::communicators::Communicator;
using gvirtus::communicators::Endpoint;

using std::chrono::steady_clock;

using namespace std;

Process::Process(
    std::shared_ptr<LD_Lib<Communicator, std::shared_ptr<Endpoint>>>
        communicator,
    vector<string> &plugins)
    : Observable() {
  logger = log4cplus::Logger::getInstance(LOG4CPLUS_TEXT("Process"));

  // Set the logging level
  log4cplus::LogLevel logLevel = log4cplus::INFO_LOG_LEVEL;
  char *val = getenv("GVIRTUS_LOGLEVEL");
  std::string logLevelString =
      (val == NULL ? std::string("") : std::string(val));
  if (logLevelString != "") {
    logLevel = std::stoi(logLevelString);
  }
  logger.setLogLevel(logLevel);

  signal(SIGCHLD, SIG_IGN);
  _communicator = communicator;
  mPlugins = plugins;
}

bool getstring(Communicator *c, string &s) {
  s = "";
  char ch = 0;
  while (c->Read(&ch, 1) == 1) {
    if (ch == 0) {
      return true;
    }
    s += ch;
  }
  return false;
}
//temporary
bool getmessage(Communicator *c, string &s){
  char* msg;
  c->Read(msg, 25);
  s = msg;
  return true;
}

extern std::string getEnvVar(std::string const &key);

std::string getGVirtuSHome() {
     std::string gvirtus_home = getEnvVar("GVIRTUS_HOME");
     return gvirtus_home;
   }

void Process::Start() {
  for_each(mPlugins.begin(), mPlugins.end(), [this](const std::string &plug) {
    std::string gvirtus_home = getGVirtuSHome();
    auto ld_path = fs::path(gvirtus_home + "/lib")
                       .append("libgvirtus-plugin-" + plug + ".so");
    try {
      auto dl = std::make_shared<LD_Lib<Handler>>(ld_path, "create_t");
      dl->build_obj();
      _handlers.push_back(dl);
    } catch (const std::string &e) {
      LOG4CPLUS_ERROR(logger, e);
    }
  });

  // 
  // put the sym of the plugins in h
  std::function<void(Communicator *)> execute = [=](Communicator *client_comm) {
    //
    //loads pointers to module symbols in mHandlers

    string routine;
    std::shared_ptr<Buffer> input_buffer = std::make_shared<Buffer>();

    while (getmessage(client_comm, routine)) {
      LOG4CPLUS_DEBUG(logger, "✓ - Received routine " << routine);

      input_buffer->Reset(client_comm);

      std::shared_ptr<Handler> h = nullptr;
      for (auto &ptr_el : _handlers) {
        if (ptr_el->obj_ptr()->CanExecute(routine)) {
          h = ptr_el->obj_ptr();
          break;
        }
      }

      std::shared_ptr<communicators::Result> result;
      if (h == nullptr) {
        LOG4CPLUS_ERROR(logger, "✖ - [Process "
                                    << getpid()
                                    << "]: Requested unknown routine "
                                    << routine << ".");
        result = std::make_shared<communicators::Result>(
            -1, std::make_shared<Buffer>());
      } else {
        auto start = steady_clock::now();
        result = h->Execute(routine, input_buffer);
        result->TimeTaken(std::chrono::duration_cast<std::chrono::milliseconds>(steady_clock::now() - start)
            .count() / 1000.0);
        
        //runs the routine and saves the result in result
      }

      
      // writes the result to the communicator
      result->Dump(client_comm);
      if (result->GetExitCode() != 0 && routine.compare("cudaLaunch")) {
        LOG4CPLUS_INFO(logger, "✓ - [Process " << getpid() << "]: Requested '"
                                                << routine << "' routine.");
        LOG4CPLUS_INFO(logger, "✓ - - [Process "
                                    << getpid() << "]: Exit Code '"
                                    << result->GetExitCode() << "'.");
      }
    }

    Notify("process-ended");
  };

  common::SignalState sig_hand;
  sig_hand.setup_signal_state(SIGINT);
  LOG4CPLUS_INFO(logger, "🛈  - process started");

  _communicator->obj_ptr()->Serve();

  LOG4CPLUS_INFO(logger, "🛈  - server is listening for requests...");

  int pid = 0;
  while (true) {
    Communicator *client =
        const_cast<Communicator *>(_communicator->obj_ptr()->Accept());
    LOG4CPLUS_INFO(logger, "🛈  - Connection accepted");
    client = _communicator->obj_ptr().get(); // added for rdma - accept doesn't return anything

    if (client != nullptr) {
      //sending _communicator instead of client here ???
      //      if ((pid = fork()) == 0) {
      std::thread(execute, client).detach();
      //        exit(0);
      //      }

    } else
      _communicator->obj_ptr()->run();

    if (common::SignalState::get_signal_state(SIGINT)) {
      LOG4CPLUS_DEBUG(logger, "✓ - SIGINT received, killing server...");
      break;
    }
  }
}

Process::~Process() {
  _communicator.reset();
  _handlers.clear();
  mPlugins.clear();
}
