#include <gtest/gtest.h>

class ExampleTest : public ::testing::Test {
protected:
  void SetUp() override {}
  void TearDown() override {}
};

TEST_F(ExampleTest, BasicAssertions) {
  // Test that true is true
  EXPECT_TRUE(true);

  // Test that false is false
  EXPECT_FALSE(false);
}

TEST_F(ExampleTest, ArithmeticAssertions) {
  // Test addition
  EXPECT_EQ(2 + 2, 4);

  // Test subtraction
  EXPECT_EQ(5 - 3, 2);
}

TEST_F(ExampleTest, StringAssertions) {
  std::string hello = "Hello, My Language!";

  // Test string equality
  EXPECT_EQ(hello, "Hello, My Language!");

  // Test string length
  EXPECT_EQ(hello.length(), 19);
}

TEST_F(ExampleTest, VectorAssertions) {
  std::vector<int> numbers = {1, 2, 3, 4, 5};

  // Test vector size
  EXPECT_EQ(numbers.size(), 5);

  // Test vector contents
  EXPECT_EQ(numbers[0], 1);
  EXPECT_EQ(numbers[4], 5);
}

TEST_F(ExampleTest, ExceptionAssertions) {
  // Test that a specific exception is thrown
  EXPECT_THROW(throw std::runtime_error("Error occurred"), std::runtime_error);

  // Test that no exception is thrown
  EXPECT_NO_THROW(int x = 42;);
}

TEST_F(ExampleTest, FloatingPointAssertions) {
  double result = 0.1 + 0.2;

  // Test floating-point equality with a tolerance
  EXPECT_NEAR(result, 0.3, 1e-9);
}

TEST_F(ExampleTest, CustomObjectAssertions) {
  struct Point {
    int x;
    int y;
  };

  Point p1 = {3, 4};
  Point p2 = {3, 4};

  // Test custom object equality
  EXPECT_EQ(p1.x, p2.x);
  EXPECT_EQ(p1.y, p2.y);
}

TEST_F(ExampleTest, BooleanAssertions) {
  bool condition = (5 > 3);

  // Test that condition is true
  EXPECT_TRUE(condition);

  condition = (2 > 3);

  // Test that condition is false
  EXPECT_FALSE(condition);
}

TEST_F(ExampleTest, NullPointerAssertions) {
  int *ptr = nullptr;

  // Test that pointer is null
  EXPECT_EQ(ptr, nullptr);

  int value = 42;
  ptr = &value;

  // Test that pointer is not null
  EXPECT_NE(ptr, nullptr);
}

TEST_F(ExampleTest, RangeAssertions) {
  int value = 10;

  // Test that value is within a specific range
  EXPECT_GE(value, 5);
  EXPECT_LE(value, 15);
}

TEST_F(ExampleTest, ContainerAssertions) {
  std::vector<int> vec = {1, 2, 3, 4, 5};

  // Test that vector contains a specific element
  EXPECT_NE(std::find(vec.begin(), vec.end(), 3), vec.end());

  // Test that vector does not contain a specific element
  EXPECT_EQ(std::find(vec.begin(), vec.end(), 6), vec.end());
}

TEST_F(ExampleTest, MultipleAssertions) {
  int a = 5;
  int b = 10;

  // Multiple assertions in a single test
  EXPECT_LT(a, b);
  EXPECT_GT(b, a);
  EXPECT_EQ(a + b, 15);
}
