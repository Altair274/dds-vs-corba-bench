// CORBA echo server (omniORB). Counterpart of the DDS "pong" side.
//
//   TCP:          corba_server -i /tmp/echo.ior -ORBendPoint giop:tcp:127.0.0.1:
//   Unix socket:  corba_server -i /tmp/echo.ior -ORBendPoint giop:unix:/tmp/echo.sock
//
// The object reference (IOR) is written to a file, so no Naming Service is
// needed. The transport is chosen with the standard -ORBendPoint option.
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include "Bench.hh"

class EchoImpl : public POA_Bench::Echo {
public:
  explicit EchoImpl(CORBA::ORB_ptr orb) : orb_(CORBA::ORB::_duplicate(orb)) {}

  Bench::Payload* roundtrip(const Bench::Payload& p) override
  {
    return new Bench::Payload(p);  // returned by value: one copy, like the DDS echo
  }

  void shutdown() override { orb_->shutdown(false); }

private:
  CORBA::ORB_var orb_;
};

int main(int argc, char** argv)
{
  try {
    CORBA::ORB_var orb = CORBA::ORB_init(argc, argv);  // consumes -ORB* options

    const char* ior_path = "echo.ior";
    for (int i = 1; i < argc; ++i) {
      if (std::strcmp(argv[i], "-i") == 0 && i + 1 < argc) ior_path = argv[++i];
    }

    CORBA::Object_var obj = orb->resolve_initial_references("RootPOA");
    PortableServer::POA_var poa = PortableServer::POA::_narrow(obj);

    PortableServer::Servant_var<EchoImpl> servant = new EchoImpl(orb);
    PortableServer::ObjectId_var oid = poa->activate_object(servant);
    CORBA::Object_var ref = poa->id_to_reference(oid);

    CORBA::String_var ior = orb->object_to_string(ref);
    const std::string tmp = std::string(ior_path) + ".tmp";
    {
      std::ofstream out(tmp);
      out << ior.in() << std::endl;
    }
    std::rename(tmp.c_str(), ior_path);  // atomic: the client never reads half a file

    PortableServer::POAManager_var mgr = poa->the_POAManager();
    mgr->activate();
    orb->run();
    orb->destroy();
  }
  catch (const CORBA::Exception& ex) {
    std::cerr << "CORBA exception: " << ex._name() << std::endl;
    return 1;
  }
  return 0;
}
