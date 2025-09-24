#include <stdio.h>
#include <math.h>
#include <GLFW/glfw3.h>

int main(void) {
  if (!glfwInit()) {
    printf("Failed to initialize GLFW\n");
    return -1;
  }

  GLFWwindow* window = glfwCreateWindow(640, 480, "spader.zone", NULL, NULL);
  if (!window) {
    printf("Failed to create GLFW window\n");
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  while (!glfwWindowShouldClose(window)) {
    float time = (float)glfwGetTime();
    glClearColor(0.64f, 0.32f, 0.5f + 0.5f * sin(time), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
