// CORBA echo client (omniORB). Counterpart of the DDS "ping" side:
// each round trip is one synchronous roundtrip() call.
//
//   corba_client -i /tmp/echo.ior -s 1024 -n 10000 -w 1000 -l corba-tcp [-r raw.txt] [-q]
//
// -q asks the server to shut down at the end of the run.
// Output: one CSV line (see common/stats.h).
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include "Bench.hh"
#include "stats.h"

int main(int argc, char** argv)
{
  try {
    CORBA::ORB_var orb = CORBA::ORB_init(argc, argv);

    std::string ior_path = "echo.ior", label = "corba";
    const char* raw_path = nullptr;
    size_t size = 64, count = 10000, warmup = 1000;
    bool quit_server = false;
    for (int i = 1; i < argc; ++i) {
      const bool has_val = i + 1 < argc;
      if (!std::strcmp(argv[i], "-i") && has_val) ior_path = argv[++i];
      else if (!std::strcmp(argv[i], "-s") && has_val) size = std::strtoull(argv[++i], nullptr, 10);
      else if (!std::strcmp(argv[i], "-n") && has_val) count = std::strtoull(argv[++i], nullptr, 10);
      else if (!std::strcmp(argv[i], "-w") && has_val) warmup = std::strtoull(argv[++i], nullptr, 10);
      else if (!std::strcmp(argv[i], "-l") && has_val) label = argv[++i];
      else if (!std::strcmp(argv[i], "-r") && has_val) raw_path = argv[++i];
      else if (!std::strcmp(argv[i], "-q")) quit_server = true;
      else {
        std::cerr << "usage: " << argv[0]
                  << " -i ior [-s bytes] [-n samples] [-w warmup] [-l label] [-r raw.txt] [-q]\n";
        return 2;
      }
    }

    std::ifstream in(ior_path);
    std::string ior;
    if (!std::getline(in, ior)) {
      std::cerr << "cannot read IOR from " << ior_path << std::endl;
      return 1;
    }

    CORBA::Object_var obj = orb->string_to_object(ior.c_str());
    Bench::Echo_var echo = Bench::Echo::_narrow(obj);
    if (CORBA::is_nil(echo)) {
      std::cerr << "IOR is not a Bench::Echo" << std::endl;
      return 1;
    }

    Bench::Payload msg;
    msg.data.length(static_cast<CORBA::ULong>(size));
    std::memset(msg.data.get_buffer(), 0, size);

    std::vector<uint64_t> rtt(count);
    for (size_t i = 0; i < warmup + count; ++i) {
      msg.seq = i;
      const uint64_t t0 = bench_now_ns();
      Bench::Payload_var reply = echo->roundtrip(msg);
      const uint64_t t1 = bench_now_ns();
      if (reply->seq != i) {
        std::cerr << "unexpected reply seq " << reply->seq << std::endl;
        return 1;
      }
      if (i >= warmup) rtt[i - warmup] = t1 - t0;
    }

    bench_report(label.c_str(), size, rtt.data(), count, raw_path);

    if (quit_server) echo->shutdown();
    orb->destroy();
  }
  catch (const CORBA::Exception& ex) {
    std::cerr << "CORBA exception: " << ex._name() << std::endl;
    return 1;
  }
  return 0;
}
