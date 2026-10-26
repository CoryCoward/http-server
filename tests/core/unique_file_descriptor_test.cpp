#include <http_server/core/unique_file_descriptor.hpp>

#include <cerrno>
#include <fcntl.h>
#include <gtest/gtest.h>
#include <system_error>
#include <unistd.h>
#include <utility>

namespace http_server::core
{

namespace
{
int create_file_descriptor()
{
    int descriptors[2]{};
    if (pipe(descriptors) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to create pipe");
    }

    if (close(descriptors[1]) != 0)
    {
        throw std::system_error(errno, std::generic_category(), "Failed to close pipe write end");
    }

    return descriptors[0];
}

bool is_closed(int fd)
{
    errno = 0;
    return fcntl(fd, F_GETFD) == -1 && errno == EBADF;
}

class UniqueFileDescriptorDeathTest : public ::testing::TestWithParam<int>
{
};
} // namespace

INSTANTIATE_TEST_SUITE_P(FileDescriptors, UniqueFileDescriptorDeathTest, ::testing::Values(-2, -1));

TEST(UniqueFileDescriptorTest, DefaultConstructionIsEmpty)
{
    // Act
    UniqueFileDescriptor descriptor;

    // Assert
    EXPECT_FALSE(descriptor.is_valid());
    EXPECT_FALSE(descriptor);
    EXPECT_EQ(descriptor.get(), -1);
    EXPECT_EQ(descriptor.release(), -1);
    descriptor.reset();
}

TEST_P(UniqueFileDescriptorDeathTest, InvalidDescriptorIsRejected)
{
    // Arrange
    const int fd = GetParam();

    // Act & Assert
    EXPECT_DEBUG_DEATH(UniqueFileDescriptor{fd}, "non-negative file descriptor");
}

TEST(UniqueFileDescriptorTest, DestructionClosesDescriptor)
{
    // Arrange
    const int fd = create_file_descriptor();

    // Act
    {
        const UniqueFileDescriptor descriptor(fd);
        EXPECT_TRUE(descriptor.is_valid());
        EXPECT_TRUE(descriptor);
        EXPECT_EQ(descriptor.get(), fd);
    } // descriptor is destroyed here

    // Assert
    EXPECT_TRUE(is_closed(fd));
}

TEST(UniqueFileDescriptorTest, ResetClosesDescriptorAndLeavesItEmpty)
{
    // Arrange
    const int fd = create_file_descriptor();
    UniqueFileDescriptor descriptor(fd);

    // Act
    descriptor.reset();

    // Assert
    EXPECT_TRUE(is_closed(fd));
    EXPECT_FALSE(descriptor.is_valid());
    EXPECT_EQ(descriptor.get(), -1);
}

TEST(UniqueFileDescriptorTest, ReleaseTransfersOwnership)
{
    // Arrange
    const int fd = create_file_descriptor();
    UniqueFileDescriptor descriptor(fd);

    // Act
    const int released_fd = descriptor.release();

    // Assert
    EXPECT_EQ(released_fd, fd);
    EXPECT_FALSE(descriptor.is_valid());
    EXPECT_NE(fcntl(released_fd, F_GETFD), -1);
    EXPECT_EQ(close(released_fd), 0);
}

TEST(UniqueFileDescriptorTest, MoveConstructionFromEmptyDescriptorRemainsEmpty)
{
    // Arrange
    UniqueFileDescriptor source;

    // Act
    const UniqueFileDescriptor destination(std::move(source));

    // Assert
    EXPECT_FALSE(destination.is_valid());
    EXPECT_EQ(destination.get(), -1);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): verifies the documented moved-from state.
    EXPECT_FALSE(source.is_valid());
}

TEST(UniqueFileDescriptorTest, MoveConstructionTransfersOwnership)
{
    // Arrange
    const int fd = create_file_descriptor();
    UniqueFileDescriptor source(fd);

    // Act
    const UniqueFileDescriptor destination(std::move(source));

    // Assert
    EXPECT_EQ(destination.get(), fd);
    EXPECT_TRUE(destination.is_valid());
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): verifies the documented moved-from state.
    EXPECT_FALSE(source.is_valid());
}

TEST(UniqueFileDescriptorTest, MoveAssignmentReleasesPreviousDescriptor)
{
    // Arrange
    const int source_fd = create_file_descriptor();
    const int destination_fd = create_file_descriptor();
    UniqueFileDescriptor source(source_fd);
    UniqueFileDescriptor destination(destination_fd);

    // Act
    destination = std::move(source);

    // Assert
    EXPECT_TRUE(is_closed(destination_fd));
    EXPECT_EQ(destination.get(), source_fd);
    // NOLINTNEXTLINE(bugprone-use-after-move,clang-analyzer-cplusplus.Move): verifies the documented moved-from state.
    EXPECT_FALSE(source.is_valid());
}

TEST(UniqueFileDescriptorTest, SelfMoveAssignmentPreservesDescriptor)
{
    // Arrange
    const int fd = create_file_descriptor();
    UniqueFileDescriptor descriptor(fd);
    UniqueFileDescriptor &same_descriptor = descriptor;

    // Act
    descriptor = std::move(same_descriptor);

    // Assert
    EXPECT_EQ(descriptor.get(), fd);
    EXPECT_TRUE(descriptor.is_valid());
    EXPECT_NE(fcntl(fd, F_GETFD), -1);
}

} // namespace http_server::core
