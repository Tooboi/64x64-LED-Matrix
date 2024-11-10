#include "led-matrix.h"
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <ctime>
#include <cmath>  // For fmod and RGB calculations

using rgb_matrix::RGBMatrix;
using rgb_matrix::Canvas;

class GameLife {
public:
  GameLife(Canvas *m, int delay_ms = 500);
  ~GameLife();
  void Run();

private:
  int numAliveNeighbours(int x, int y);
  void updateValues();
  void InitRandomValues();
  void HSVtoRGB(float hue, float saturation, float brightness, int &r, int &g, int &b);
  void addGlider();  // Function to add a glider to the grid

  Canvas *canvas_;
  int delay_ms_;
  int width_;
  int height_;
  float hueShift_;
  int** values_;
  int** newValues_;
  time_t lastGliderTime_;  // Track the last time a glider was added
};

GameLife::GameLife(Canvas *m, int delay_ms)
    : canvas_(m), delay_ms_(delay_ms), hueShift_(0.0f), lastGliderTime_(time(0)) {
  width_ = canvas_->width();
  height_ = canvas_->height();

  if (width_ < 5 || height_ < 5) {
    printf("Grid is too small for Game of Life. Please use a grid size of at least 5x5.\n");
    exit(1);
  }

  values_ = new int*[width_];
  newValues_ = new int*[width_];
  for (int x = 0; x < width_; ++x) {
    values_[x] = new int[height_];
    newValues_[x] = new int[height_];
  }

  InitRandomValues();  
}

GameLife::~GameLife() {
  for (int x = 0; x < width_; ++x) {
    delete[] values_[x];
    delete[] newValues_[x];
  }
  delete[] values_;
  delete[] newValues_;
}

void GameLife::Run() {
  while (true) {
    // Add a glider every 8 seconds
    time_t currentTime = time(0);
    if (currentTime - lastGliderTime_ >= 1) {
      addGlider();  // Add the glider
      lastGliderTime_ = currentTime;  // Update last glider time
    }

    updateValues();

    for (int x = 0; x < width_; ++x) {
      for (int y = 0; y < height_; ++y) {
        int colorR, colorG, colorB;
        if (values_[x][y]) {
          float distX = float(x - width_ / 2);
          float distY = float(y - height_ / 2);
          float distance = sqrt(distX * distX + distY * distY);
          float maxDist = sqrt((width_ / 2) * (width_ / 2) + (height_ / 2) * (height_ / 2));
          float normalizedDist = distance / maxDist;
          float smoothDist = 1.0f - pow(1.0f - normalizedDist, 3.0f);
          float hue = 160.0f - (smoothDist * 50.0f);
          float saturation = 0.8f;
          float brightness = 1.0f - (smoothDist * 0.0f);
          float shiftedHue = fmod(hue + hueShift_, 360.0f);
          HSVtoRGB(shiftedHue, saturation, brightness, colorR, colorG, colorB);
          canvas_->SetPixel(x, y, colorR, colorG, colorB);
        } else {
          canvas_->SetPixel(x, y, 0, 0, 0);
        }
      }
    }

    hueShift_ += 0.5f;
    if (hueShift_ >= 360.0f) {
      hueShift_ = 0.0f;
    }

    usleep(delay_ms_ * 1000); 
  }
}

void GameLife::addGlider() {
  // Randomly choose a starting position for the glider
  int startX = rand() % (width_ - 5);  // Ensure there's enough space for the glider
  int startY = rand() % (height_ - 5);

  // Glider pattern (relative positions of live cells)
  int glider[5][5] = {
    {0, 0, 1, 0, 0},
    {0, 0, 0, 1, 1},
    {1, 1, 0, 1, 0},
    {0, 0, 1, 1, 0},
    {0, 0, 0, 0, 0}
  };

  // Set the glider pattern on the grid
  for (int x = 0; x < 5; ++x) {
    for (int y = 0; y < 5; ++y) {
      if (glider[x][y] == 1) {
        values_[(startX + x) % width_][(startY + y) % height_] = 1;
      }
    }
  }
}

int GameLife::numAliveNeighbours(int x, int y) {
  int num = 0;
  for (int i = -1; i <= 1; ++i) {
    for (int j = -1; j <= 1; ++j) {
      if (i == 0 && j == 0) continue;
      int nx = (x + i + width_) % width_;
      int ny = (y + j + height_) % height_;
      num += values_[nx][ny];
    }
  }
  return num;
}

void GameLife::updateValues() {
  for (int x = 0; x < width_; ++x) {
    for (int y = 0; y < height_; ++y) {
      newValues_[x][y] = values_[x][y];
    }
  }

  for (int x = 0; x < width_; ++x) {
    for (int y = 0; y < height_; ++y) {
      int num = numAliveNeighbours(x, y);
      if (values_[x][y]) {
        if (num < 2 || num > 3) newValues_[x][y] = 0;
      } else {
        if (num == 3) newValues_[x][y] = 1;
      }
    }
  }

  for (int x = 0; x < width_; ++x) {
    for (int y = 0; y < height_; ++y) {
      values_[x][y] = newValues_[x][y];
    }
  }
}

void GameLife::InitRandomValues() {
  for (int x = 0; x < width_; ++x) {
    for (int y = 0; y < height_; ++y) {
      values_[x][y] = rand() % 2;
    }
  }
}

void GameLife::HSVtoRGB(float hue, float saturation, float brightness, int &r, int &g, int &b) {
  float c = brightness * saturation;
  float x = c * (1 - fabs(fmod(hue / 60.0f, 2) - 1));
  float m = brightness - c;

  float r1 = 0, g1 = 0, b1 = 0;

  if (hue >= 0 && hue < 60) {
    r1 = c, g1 = x, b1 = 0;
  } else if (hue >= 60 && hue < 120) {
    r1 = x, g1 = c, b1 = 0;
  } else if (hue >= 120 && hue < 180) {
    r1 = 0, g1 = c, b1 = x;
  } else if (hue >= 180 && hue < 240) {
    r1 = 0, g1 = x, b1 = c;
  } else if (hue >= 240 && hue < 300) {
    r1 = x, g1 = 0, b1 = c;
  } else if (hue >= 300 && hue < 360) {
    r1 = c, g1 = 0, b1 = x;
  }

  r = (r1 + m) * 255;
  g = (g1 + m) * 255;
  b = (b1 + m) * 255;
}

int main(int argc, char *argv[]) {
  RGBMatrix::Options defaults;
  defaults.hardware_mapping = "adafruit-hat-pwm";
  defaults.rows = 64;
  defaults.cols = 64;
  defaults.chain_length = 1;
  defaults.parallel = 1;
  defaults.show_refresh_rate = true;

  Canvas *canvas = rgb_matrix::RGBMatrix::CreateFromFlags(&argc, &argv, &defaults);
  if (canvas == NULL) return 1;

  GameLife game(canvas, 100);  
  game.Run();

  delete canvas;
  return 0;
}