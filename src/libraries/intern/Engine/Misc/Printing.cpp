#include "Printing.hpp"

#include <glm/gtc/matrix_access.hpp>
#include <glm/gtx/string_cast.hpp>

#include <iostream>

void printMatRows(glm::mat4 projection)
{
    std::cout << "----------------------" << std::endl;
    auto row0 = glm::row(projection, 0);
    auto row1 = glm::row(projection, 1);
    auto row2 = glm::row(projection, 2);
    auto row3 = glm::row(projection, 3);
    std::cout << glm::to_string(row0) << std::endl;
    std::cout << glm::to_string(row1) << std::endl;
    std::cout << glm::to_string(row2) << std::endl;
    std::cout << glm::to_string(row3) << std::endl;
};