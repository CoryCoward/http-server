#include <http_server/core/unique_file_descriptor.hpp>
#include <http_server/tcp/tcp_server.hpp>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <unistd.h>

namespace http_server::tcp
{

using core::UniqueFileDescriptor;

TEST(TcpServerTest, StartWithEphemeralPortSucceeds)
{
    // Arrange
    TcpServer server;
    const TcpServerConfig config{.port = 0, .max_backlog = 1};

    // Act
    const auto result = server.start(config);

    // Assert
    EXPECT_TRUE(result);
}

TEST(TcpServerTest, StartReturnsFailureWhenCalledTwice)
{
    // Arrange
    TcpServer server;
    const TcpServerConfig config{.port = 0, .max_backlog = 1};

    ASSERT_TRUE(server.start(config));

    // Act
    const auto result = server.start(config);

    // Assert
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ServerErrorCode::already_running);
    EXPECT_FALSE(result.error().socket_error.has_value());
}

TEST(TcpServerTest, AcceptingWhenStoppedReturnsNotRunning)
{
    // Arrange
    TcpServer server;

    // Act
    const auto result = server.accept_connection();

    // Assert
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ServerErrorCode::not_running);
    EXPECT_FALSE(result.error().socket_error.has_value());
}

TEST(TcpServerTest, StartWithBoundPortReturnsSocketFailure)
{
    // Arrange
    UniqueFileDescriptor reservation(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(reservation.is_valid());

    sockaddr_in listener_address{};
    listener_address.sin_family = AF_INET;
    listener_address.sin_addr.s_addr = htonl(INADDR_ANY);
    ASSERT_EQ(
        ::bind(reservation.get(), reinterpret_cast<const sockaddr *>(&listener_address), sizeof(listener_address)), 0);
    ASSERT_EQ(::listen(reservation.get(), 1), 0);

    socklen_t listener_address_length = sizeof(listener_address);
    ASSERT_EQ(getsockname(reservation.get(), reinterpret_cast<sockaddr *>(&listener_address), &listener_address_length),
              0);

    TcpServer server;

    // Act
    const auto result = server.start(TcpServerConfig{.port = ntohs(listener_address.sin_port), .max_backlog = 1});

    // Assert
    ASSERT_FALSE(result);
    EXPECT_EQ(result.error().code, ServerErrorCode::socket_failure);
    ASSERT_TRUE(result.error().socket_error.has_value());
    // NOLINTNEXTLINE(bugprone-unchecked-optional-access): guarded by the preceding ASSERT_TRUE.
    EXPECT_EQ(result.error().socket_error->code, SocketErrorCode::system_error);
}

TEST(TcpServerTest, AcceptingConnectionReturnsConnectedSocket)
{
    // Arrange
    UniqueFileDescriptor reservation(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(reservation.is_valid());

    sockaddr_in listener_address{};
    listener_address.sin_family = AF_INET;
    listener_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ASSERT_EQ(
        ::bind(reservation.get(), reinterpret_cast<const sockaddr *>(&listener_address), sizeof(listener_address)), 0);

    socklen_t listener_address_length = sizeof(listener_address);
    ASSERT_EQ(getsockname(reservation.get(), reinterpret_cast<sockaddr *>(&listener_address), &listener_address_length),
              0);
    reservation.reset();

    TcpServer server;
    ASSERT_TRUE(server.start(TcpServerConfig{.port = ntohs(listener_address.sin_port), .max_backlog = 1}));

    UniqueFileDescriptor client_socket(::socket(AF_INET, SOCK_STREAM, 0));
    ASSERT_TRUE(client_socket.is_valid());
    ASSERT_EQ(
        ::connect(client_socket.get(), reinterpret_cast<const sockaddr *>(&listener_address), sizeof(listener_address)),
        0);

    // Act
    const auto result = server.accept_connection();

    // Assert
    ASSERT_TRUE(result);
    EXPECT_TRUE(result->is_valid());
    EXPECT_GE(result->get(), 0);
}

TEST(TcpServerTest, StopAllowsServerToRestart)
{
    // Arrange
    TcpServer server;
    const TcpServerConfig config{.port = 0, .max_backlog = 1};
    ASSERT_TRUE(server.start(config));

    // Act
    server.stop();

    // Assert
    EXPECT_FALSE(server.is_running());
    EXPECT_TRUE(server.start(config));
    EXPECT_TRUE(server.is_running());
}

} // namespace http_server::tcp
