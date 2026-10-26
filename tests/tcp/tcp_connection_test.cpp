#include <http_server/core/unique_file_descriptor.hpp>
#include <http_server/tcp/tcp_connection.hpp>

#include "support/tcp_connection_test_factory.hpp"

#include <algorithm>
#include <arpa/inet.h>
#include <cstdint>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <optional>
#include <string>
#include <sys/socket.h>
#include <utility>

namespace http_server::tcp
{

using core::UniqueFileDescriptor;

struct IpAddressTestCase
{
    std::optional<std::string> input;
    std::string expected;
};

struct PortTestCase
{
    std::optional<std::uint16_t> input;
    std::uint16_t expected;
};

class TcpConnectionIpAddressTest : public ::testing::TestWithParam<IpAddressTestCase>
{
};

class TcpConnectionPortTest : public ::testing::TestWithParam<PortTestCase>
{
};

INSTANTIATE_TEST_SUITE_P(IpAddresses,
                         TcpConnectionIpAddressTest,
                         ::testing::Values(IpAddressTestCase{std::nullopt, "0.0.0.0"},
                                           IpAddressTestCase{std::string{"203.0.113.42"}, "203.0.113.42"}),
                         [](const ::testing::TestParamInfo<IpAddressTestCase> &info)
                         {
                             if (!info.param.input)
                             {
                                 return std::string{"UnspecifiedAddress"};
                             }

                             auto name = *info.param.input;
                             std::ranges::replace(name, '.', '_');

                             return name;
                         });

INSTANTIATE_TEST_SUITE_P(Ports,
                         TcpConnectionPortTest,
                         ::testing::Values(PortTestCase{0, 0}, PortTestCase{1234, 1234}),
                         [](const ::testing::TestParamInfo<PortTestCase> &info)
                         {
                             return info.param.input ? std::to_string(*info.param.input)
                                                     : std::string{"UnspecifiedPort"};
                         });

TEST_P(TcpConnectionIpAddressTest, ReturnsCorrectIpAddress)
{
    // Arrange
    const auto &[input, expected] = GetParam();

    sockaddr_in client_address{};
    client_address.sin_family = AF_INET;

    if (input.has_value())
    {
        ASSERT_EQ(::inet_pton(AF_INET, input.value().c_str(), &client_address.sin_addr), 1);
    }

    const auto connection = TcpConnectionTestFactory::create(UniqueFileDescriptor{}, client_address);

    // Act
    const auto result = connection.get_client_ip_address();

    // Assert
    ASSERT_TRUE(result);
    ASSERT_EQ(*result, expected);
}

TEST_P(TcpConnectionPortTest, ReturnsCorrectPort)
{
    // Arrange
    const auto &[input, expected] = GetParam();

    sockaddr_in client_address{};
    client_address.sin_family = AF_INET;

    if (input.has_value())
    {
        client_address.sin_port = htons(input.value());
    }

    const auto connection = TcpConnectionTestFactory::create(UniqueFileDescriptor{}, client_address);

    // Act
    const auto result = connection.get_client_port();

    // Assert
    ASSERT_EQ(result, expected);
}

TEST(TcpConnectionTest, DefaultDescriptorIsInvalid)
{
    // Arrange
    sockaddr_in client_address{};

    // Act
    const auto connection = TcpConnectionTestFactory::create(UniqueFileDescriptor{}, client_address);

    // Assert
    EXPECT_FALSE(connection.is_valid());
    EXPECT_EQ(connection.get(), -1);
}

TEST(TcpConnectionTest, RetainsValidConnectedDescriptor)
{
    // Arrange
    const auto sock = ::socket(AF_INET, SOCK_STREAM, 0);
    UniqueFileDescriptor fd{sock};

    sockaddr_in client_address{};

    // Act
    const auto connection = TcpConnectionTestFactory::create(std::move(fd), client_address);

    // Assert
    ASSERT_TRUE(connection.is_valid());
    EXPECT_GE(connection.get(), 0);
}

} // namespace http_server::tcp
