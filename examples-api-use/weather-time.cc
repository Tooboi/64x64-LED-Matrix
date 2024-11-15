#include <iostream>
#include "led-matrix.h"
#include "graphics.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <jsoncpp/json/json.h>
#include <cmath>
#include <ctime>
#include <signal.h>
#include <thread>
#include <Magick++.h>
#include <vector>

using namespace rgb_matrix;
using ImageVector = std::vector<Magick::Image>;

volatile bool running = true;

// Function declarations
static int usage(const char *progname);
static bool parseColor(Color *c, const char *str);
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp);
float getTemperature(std::string &date_time, int &weather_code);
void handle_signal(int signal);
void drawDisplay(RGBMatrix* canvas, const rgb_matrix::Font& font, 
                rgb_matrix::Font* outline_font, const Color& color, 
                const Color& bg_color, const Color& flood_color,
                const char* temp_message, const char* time_message, const char* date_message,
                const char* weather_message,  // New parameter for weather info
                int letter_spacing, int base_y_orig);

// Function implementations
static int usage(const char *progname) {
    fprintf(stderr, "usage: %s [options]\n", progname);
    fprintf(stderr, "Displays a static message on the LED matrix.\n");
    fprintf(stderr, "Options:\n");
    fprintf(stderr,
            "\t-f <font-file>    : Use given font.\n"
            "\t-x <x-origin>     : X-Origin of displaying text (Default: 0)\n"
            "\t-y <y-origin>     : Y-Origin of displaying text (Default: 0)\n"
            "\t-S <spacing>      : Spacing pixels between letters (Default: 0)\n"
            "\t-C <r,g,b>        : Color. Default 255,255,0\n"
            "\t-B <r,g,b>        : Font Background-Color. Default 0,0,0\n"
            "\t-O <r,g,b>        : Outline-Color, e.g., to increase contrast.\n"
            "\t-F <r,g,b>        : Panel flooding-background color. Default 0,0,0\n"
            "\n");
    rgb_matrix::PrintMatrixFlags(stderr);
    return 1;
}

static bool parseColor(Color *c, const char *str) {
    return sscanf(str, "%hhu,%hhu,%hhu", &c->r, &c->g, &c->b) == 3;
}

size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

float getTemperature(std::string &date_time, int &weather_code) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;
    float temperature = 0.0;

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "https://api.open-meteo.com/v1/forecast?latitude=40.6012&longitude=-74.559&current=temperature_2m,weather_code&temperature_unit=fahrenheit&timezone=America%2FNew_York");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            Json::CharReaderBuilder reader;
            Json::Value obj;
            std::istringstream s(readBuffer);
            std::string errs;

            if (Json::parseFromStream(reader, s, &obj, &errs)) {
                if (obj.isMember("current")) {
                    Json::Value current = obj["current"];
                    
                    // Get all values in one block
                    if (current.isMember("temperature_2m") && 
                        current.isMember("time") && 
                        current.isMember("weather_code")) {
                        
                        temperature = current["temperature_2m"].asFloat();
                        date_time = current["time"].asString();
                        weather_code = current["weather_code"].asInt();
                        
                        // Remove debug prints from here, they'll be handled in main loop
                    } else {
                        std::cerr << "Missing required fields in API response" << std::endl;
                    }
                } else {
                    std::cerr << "No 'current' object in API response" << std::endl;
                }
            } else {
                std::cerr << "Error parsing JSON response: " << errs << std::endl;
            }
        } else {
            std::cerr << "Error fetching data from API: " << curl_easy_strerror(res) << std::endl;
        }
        curl_easy_cleanup(curl);
    }

    curl_global_cleanup();
    return temperature;
}




void handle_signal(int signal) {
    std::cerr << "Program interrupted, exiting..." << std::endl;
    running = false;
}

void drawDisplay(RGBMatrix* canvas, const rgb_matrix::Font& font, 
                rgb_matrix::Font* outline_font, const Color& color, 
                const Color& bg_color, const Color& flood_color,
                const char* temp_message, const char* time_message, const char* date_message,
                const char* weather_message,  // New parameter for weather info
                int letter_spacing, int base_y_orig) {

                    

    canvas->Fill(flood_color.r, flood_color.g, flood_color.b);

    int char_width = 5;
    int temp_length = strlen(temp_message);
    int time_length = strlen(time_message);
    int date_length = strlen(date_message);
    int weather_length = strlen(weather_message);  // Weather message length

    int temp_width = temp_length * char_width + (temp_length - 1) * letter_spacing;
    int time_width = time_length * char_width + (time_length - 1) * letter_spacing;
    int date_width = date_length * char_width + (date_length - 1) * letter_spacing;
    int weather_width = weather_length * char_width + (weather_length - 1) * letter_spacing;

    int time_x = (canvas->width() - time_width) / 2;
    int temp_x = (canvas->width() - temp_width) / 2;
    int date_x = (canvas->width() - date_width) / 2;
    int weather_x = (canvas->width() - weather_width) / 2;  // Center weather message horizontally

    int screen_center_y = canvas->height() / 2;
    int time_y = screen_center_y - font.height() / 2;
    int temp_y = time_y - font.height() - 2;
    int date_y = time_y + font.height() + 2;
    int weather_y = screen_center_y + font.height() + 12;  // Position for weather message

    if (outline_font) {
        rgb_matrix::DrawText(canvas, *outline_font, temp_x, temp_y + font.baseline(), color, NULL, temp_message, letter_spacing);
        rgb_matrix::DrawText(canvas, *outline_font, time_x, time_y + font.baseline(), color, NULL, time_message, letter_spacing);
        rgb_matrix::DrawText(canvas, *outline_font, date_x, date_y + font.baseline(), color, NULL, date_message, letter_spacing);
        rgb_matrix::DrawText(canvas, *outline_font, weather_x, weather_y + font.baseline(), color, NULL, weather_message, letter_spacing);
    }

    rgb_matrix::DrawText(canvas, font, temp_x, temp_y + font.baseline(), color, outline_font ? NULL : &bg_color, temp_message, letter_spacing);
    rgb_matrix::DrawText(canvas, font, time_x, time_y + font.baseline(), color, outline_font ? NULL : &bg_color, time_message, letter_spacing);
    rgb_matrix::DrawText(canvas, font, date_x, date_y + font.baseline(), color, outline_font ? NULL : &bg_color, date_message, letter_spacing);
    rgb_matrix::DrawText(canvas, font, weather_x, weather_y + font.baseline(), color, outline_font ? NULL : &bg_color, weather_message, letter_spacing);
}

time_t last_weather_update = 0;
const int WEATHER_UPDATE_INTERVAL = 900;  // 15 minutes in seconds
int last_minute = -1;  // Track last minute we updated

int main(int argc, char *argv[]) {
    signal(SIGINT, handle_signal);

    RGBMatrix::Options matrix_options;
    rgb_matrix::RuntimeOptions runtime_opt;
    if (!rgb_matrix::ParseOptionsFromFlags(&argc, &argv, &matrix_options, &runtime_opt)) {
        return usage(argv[0]);
    }

    Color color(0, 255, 0);
    Color bg_color(0, 0, 0);
    Color flood_color(0, 0, 0);
    Color outline_color(0, 0, 0);
    bool with_outline = false;

    const char *bdf_font_file = NULL;
    int letter_spacing = 0;
    int y_orig = 32 - 7;

    int opt;
    while ((opt = getopt(argc, argv, "x:y:f:C:B:O:S:F:")) != -1) {
        switch (opt) {
            case 'y': y_orig = atoi(optarg); break;
            case 'f': bdf_font_file = strdup(optarg); break;
            case 'S': letter_spacing = atoi(optarg); break;
            case 'C': if (!parseColor(&color, optarg)) return usage(argv[0]); break;
            case 'B': if (!parseColor(&bg_color, optarg)) return usage(argv[0]); break;
            case 'O': if (!parseColor(&outline_color, optarg)) { return usage(argv[0]); } with_outline = true; break;
            case 'F': if (!parseColor(&flood_color, optarg)) { return usage(argv[0]); } break;
            default: return usage(argv[0]);
        }
    }

    if (bdf_font_file == NULL) {
        fprintf(stderr, "Need to specify BDF font-file with -f\n");
        return usage(argv[0]);
    }

    rgb_matrix::Font font;
    if (!font.LoadFont(bdf_font_file)) {
        fprintf(stderr, "Couldn't load font '%s'\n", bdf_font_file);
        return 1;
    }

    rgb_matrix::Font *outline_font = NULL;
    if (with_outline) {
        outline_font = font.CreateOutlineFont();
    }

    // Create the canvas for the LED matrix
    RGBMatrix *canvas = RGBMatrix::CreateFromOptions(matrix_options, runtime_opt);
    if (canvas == NULL) return 1;

    // Clear the display to avoid random LED flashes at startup
    canvas->Clear();

    // Optional: Add a very brief sleep to allow for system settling
    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    // Clear the display again to ensure there's no leftover garbage data
    canvas->Clear();

    // Fetch weather data
    std::string date_time;
    int weather_code;
    float temperature = getTemperature(date_time, weather_code);
    char temperature_message[100];
    snprintf(temperature_message, sizeof(temperature_message), "%dF", static_cast<int>(std::round(temperature)));

    char time_message[100];
    char date_message[100];
    time_t current_time;
    struct tm *tm_info;

    time(&current_time);
    tm_info = localtime(&current_time);
    strftime(time_message, sizeof(time_message), "%l:%M %p", tm_info);
    strftime(date_message, sizeof(date_message), "%b %d %Y", tm_info);

   // Set up weather message based on weather code
    char weather_message[100];
    switch (weather_code) {
        case 0: snprintf(weather_message, sizeof(weather_message), "Clear"); break;
        case 1: snprintf(weather_message, sizeof(weather_message), "Mostly Clear"); break;
        case 2: snprintf(weather_message, sizeof(weather_message), "Partly Cloudy"); break;
        case 3: snprintf(weather_message, sizeof(weather_message), "Overcast"); break;
        default: snprintf(weather_message, sizeof(weather_message), "Unknown"); break;
    }

    int base_y = y_orig;
    time_t last_weather_update = current_time;  // Track last weather update
    const int WEATHER_UPDATE_INTERVAL = 900;    // 15 minutes in seconds
    int last_minute = -1;                       // Track last minute we updated

    while (running) {
        time(&current_time);
        tm_info = localtime(&current_time);
        
        // Check if minute has changed
        if (tm_info->tm_min != last_minute) {
            strftime(time_message, sizeof(time_message), "%l:%M %p", tm_info);
            strftime(date_message, sizeof(date_message), "%b %d %Y", tm_info);
            last_minute = tm_info->tm_min;
        }

        // Check if it's time to update weather (every 15 minutes)
        if (current_time - last_weather_update >= WEATHER_UPDATE_INTERVAL) {
            std::cout << "Fetching weather update..." << std::endl;
            temperature = getTemperature(date_time, weather_code);
            snprintf(temperature_message, sizeof(temperature_message), "%dF", 
                    static_cast<int>(std::round(temperature)));
            
            // Update weather message based on weather code
            switch (weather_code) {
                case 0: snprintf(weather_message, sizeof(weather_message), "Clear"); break;
                case 1: snprintf(weather_message, sizeof(weather_message), "Mostly Clear"); break;
                case 2: snprintf(weather_message, sizeof(weather_message), "Partly Cloudy"); break;
                case 3: snprintf(weather_message, sizeof(weather_message), "Overcast"); break;
                case 45: snprintf(weather_message, sizeof(weather_message), "Foggy"); break;
                case 48: snprintf(weather_message, sizeof(weather_message), "Icy Fog"); break;
                case 51: snprintf(weather_message, sizeof(weather_message), "Light Drizzle"); break;
                case 53: snprintf(weather_message, sizeof(weather_message), "Drizzle"); break;
                case 55: snprintf(weather_message, sizeof(weather_message), "Heavy Drizzle"); break;
                case 61: snprintf(weather_message, sizeof(weather_message), "Light Rain"); break;
                case 63: snprintf(weather_message, sizeof(weather_message), "Rain"); break;
                case 65: snprintf(weather_message, sizeof(weather_message), "Heavy Rain"); break;
                case 71: snprintf(weather_message, sizeof(weather_message), "Light Snow"); break;
                case 73: snprintf(weather_message, sizeof(weather_message), "Snow"); break;
                case 75: snprintf(weather_message, sizeof(weather_message), "Heavy Snow"); break;
                case 80: snprintf(weather_message, sizeof(weather_message), "Light Showers"); break;
                case 81: snprintf(weather_message, sizeof(weather_message), "Showers"); break;
                case 82: snprintf(weather_message, sizeof(weather_message), "Heavy Showers"); break;
                case 85: snprintf(weather_message, sizeof(weather_message), "Snow Showers"); break;
                case 86: snprintf(weather_message, sizeof(weather_message), "Heavy Snow"); break;
                case 95: snprintf(weather_message, sizeof(weather_message), "Thunderstorm"); break;
                case 96:
                case 99: snprintf(weather_message, sizeof(weather_message), "Thunder/Hail"); break;
                default: snprintf(weather_message, sizeof(weather_message), "Unknown (%d)", weather_code); break;
            }
            
            last_weather_update = current_time;
            std::cout << "-----------------------" << std::endl;
            std::cout << "Weather updated at: " << ctime(&current_time);
            std::cout << "Temperature: " << temperature << "F" << std::endl;
            std::cout << "Weather Code: " << weather_code << std::endl;
            std::cout << "-----------------------" << std::endl;
        }

        drawDisplay(canvas, font, outline_font, color, bg_color, flood_color, 
            temperature_message, time_message, date_message, weather_message, 
            letter_spacing, base_y);

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    delete canvas;
    if (outline_font) {
        delete outline_font;
    }
    delete[] bdf_font_file;
    return 0;
}