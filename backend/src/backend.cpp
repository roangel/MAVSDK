#include "backend.h"

#include <grpc/grpc.h>
#include <grpc++/server.h>
#include <grpc++/server_builder.h>
#include <grpc++/server_context.h>
#include <grpc++/security/server_credentials.h>
#include <memory>
#include <mutex>

#include "action/action.h"
#include "action/action_service_impl.h"
#include "connection_initiator.h"
#include "core/core_service_impl.h"
#include "dronecore.h"
#include "log.h"
#include "mission/mission.h"
#include "mission/mission_service_impl.h"
#include "telemetry/telemetry_service_impl.h"

namespace dronecore {
namespace backend {

class DroneCoreBackend::Impl
{
public:
    Impl() {}
    ~Impl() {}

    bool run(const int mavlink_listen_port)
    {
        _connection_initiator.start(_dc, 14540);
        _connection_initiator.wait();

        grpc::ServerBuilder builder;
        setup_port(builder);

        CoreServiceImpl<> core(_dc);
        builder.RegisterService(&core);

        Action action(_dc.system());
        ActionServiceImpl<> action_service(action);
        builder.RegisterService(&action_service);

        Mission mission(_dc.system());
        MissionServiceImpl<> mission_service(mission);
        builder.RegisterService(&mission_service);

        Telemetry telemetry(_dc.system());
        TelemetryServiceImpl<> telemetry_service(telemetry);
        builder.RegisterService(&telemetry_service);

        _server = builder.BuildAndStart();
        LogInfo() << "Server started";
        _server->Wait();

        return true;
    }

private:
    void setup_port(grpc::ServerBuilder &builder)
    {
        std::string server_address("0.0.0.0:50051");
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        LogInfo() << "Server set to listen on " << server_address;
    }

    dronecore::DroneCore _dc;
    dronecore::backend::ConnectionInitiator<dronecore::DroneCore> _connection_initiator;
    std::unique_ptr<grpc::Server> _server;
};

DroneCoreBackend::DroneCoreBackend() : _impl(new Impl()) {}
DroneCoreBackend::~DroneCoreBackend() = default;

bool DroneCoreBackend::run(const int mavlink_listen_port) { return _impl->run(mavlink_listen_port); }

} // namespace backend
} // namespace dronecore