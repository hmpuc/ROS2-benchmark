cmake_minimum_required(VERSION 3.8)
project(node_central)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Dependências
find_package(ament_cmake REQUIRED)
find_package(rclcpp REQUIRED)
find_package(cassio_interface REQUIRED)

# Criar executável (ajusta o nome do .cpp se for diferente)
add_executable(node_central src/node_central.cpp)

# Linkar dependências
ament_target_dependencies(node_central rclcpp cassio_interface)

# Instalar executável (ESSENCIAL pro ros2 run funcionar)
install(TARGETS
  node_central
  DESTINATION lib/${PROJECT_NAME}
)

# Testes (mantém como estava)
if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  set(ament_cmake_copyright_FOUND TRUE)
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()