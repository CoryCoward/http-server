#include <http_server/config/http_server_config_loader.hpp>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <stdexcept>
#include <string_view>

namespace http_server::config
{

namespace
{
class TemporaryConfigFile
{
public:
    explicit TemporaryConfigFile(std::string_view contents)
        : m_path(std::filesystem::temp_directory_path() / "http-server-config-loader-test.toml")
    {
        std::ofstream config_file{m_path};

        if (!config_file)
        {
            throw std::runtime_error("Failed to create temporary configuration file");
        }

        config_file << contents;

        if (!config_file)
        {
            throw std::runtime_error("Failed to write temporary configuration file");
        }
    }

    ~TemporaryConfigFile()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    [[nodiscard]] const std::filesystem::path &path() const { return m_path; }

private:
    std::filesystem::path m_path;
};
} // namespace

TEST(HttpServerConfigLoaderTest, LoadsDefaultConfiguration)
{
    // Arrange
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load();

    // Assert
    ASSERT_TRUE(result) << result.error().diagnostic;
    EXPECT_TRUE(config_loader.is_loaded());
    EXPECT_EQ(config_loader.config().tcp_server.port, 8080);
    EXPECT_EQ(config_loader.config().tcp_server.max_backlog, 5);
}

TEST(HttpServerConfigLoaderTest, LoadsSpecifiedConfiguration)
{
    // Arrange
    const TemporaryConfigFile config_file{"[tcp_server]\n"
                                          "port = 9090\n"
                                          "max_backlog = 10\n"};
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load(config_file.path());

    // Assert
    ASSERT_TRUE(result) << result.error().diagnostic;
    EXPECT_TRUE(config_loader.is_loaded());
    EXPECT_EQ(config_loader.config().tcp_server.port, 9090);
    EXPECT_EQ(config_loader.config().tcp_server.max_backlog, 10);
}

TEST(HttpServerConfigLoaderTest, RejectsMissingRequiredValue)
{
    // Arrange
    const TemporaryConfigFile config_file{"[tcp_server]\n"
                                          "port = 8080\n"};
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load(config_file.path());

    // Assert
    ASSERT_FALSE(result);
    EXPECT_FALSE(config_loader.is_loaded());
    EXPECT_EQ(result.error().code, ConfigErrorCode::missing_required_value);
    EXPECT_EQ(result.error().key, "tcp_server.max_backlog");
}

TEST(HttpServerConfigLoaderTest, RejectsOutOfRangePort)
{
    // Arrange
    const TemporaryConfigFile config_file{"[tcp_server]\n"
                                          "port = 65536\n"
                                          "max_backlog = 5\n"};
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load(config_file.path());

    // Assert
    ASSERT_FALSE(result);
    EXPECT_FALSE(config_loader.is_loaded());
    EXPECT_EQ(result.error().code, ConfigErrorCode::value_out_of_range);
    EXPECT_EQ(result.error().key, "tcp_server.port");
}

TEST(HttpServerConfigLoaderTest, RejectsValueWithWrongType)
{
    // Arrange
    const TemporaryConfigFile config_file{"[tcp_server]\n"
                                          "port = \"8080\"\n"
                                          "max_backlog = 5\n"};
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load(config_file.path());

    // Assert
    ASSERT_FALSE(result);
    EXPECT_FALSE(config_loader.is_loaded());
    EXPECT_EQ(result.error().code, ConfigErrorCode::invalid_value);
    EXPECT_EQ(result.error().key, "tcp_server.port");
}

TEST(HttpServerConfigLoaderTest, RejectsMissingConfigurationFile)
{
    // Arrange
    const std::filesystem::path missing_path =
        std::filesystem::temp_directory_path() / "http-server-missing-config.toml";
    std::error_code error;
    std::filesystem::remove(missing_path, error);
    HttpServerConfigLoader config_loader;

    // Act
    const auto result = config_loader.load(missing_path);

    // Assert
    ASSERT_FALSE(result);
    EXPECT_FALSE(config_loader.is_loaded());
    EXPECT_EQ(result.error().code, ConfigErrorCode::parse_failure);
    EXPECT_EQ(result.error().config_path, missing_path);
}

TEST(HttpServerConfigLoaderTest, FailedReloadClearsPreviousConfiguration)
{
    // Arrange
    HttpServerConfigLoader config_loader;

    {
        const TemporaryConfigFile valid_config{"[tcp_server]\n"
                                               "port = 9090\n"
                                               "max_backlog = 10\n"};
        ASSERT_TRUE(config_loader.load(valid_config.path()));
    }

    const TemporaryConfigFile invalid_config{"[tcp_server]\n"
                                             "port = 9090\n"};

    // Act
    const auto result = config_loader.load(invalid_config.path());

    // Assert
    ASSERT_FALSE(result);
    EXPECT_FALSE(config_loader.is_loaded());
    EXPECT_EQ(config_loader.config().tcp_server.port, 0);
    EXPECT_EQ(config_loader.config().tcp_server.max_backlog, 0);
}

} // namespace http_server::config
