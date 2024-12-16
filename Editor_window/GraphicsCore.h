#pragma once
#include <windows.h>
#include <vector>
#include <cmath>
#include <iostream>
#include <thread>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct vec3d {
    float x, y, z;
    vec3d(float x, float y, float z) : x(x), y(y), z(z) {}

    vec3d projectTo2D(int centerX, int centerY, float scale, float moveX, float moveY) const {
        const float fov = 70.0f;
        const float aspectRatio = 16.0f / 9.0f;

        float projectedX = x / (1 + z / (fov * scale));
        float projectedY = y / (1 + z / (fov * scale));

        projectedX *= aspectRatio;
        projectedY *= aspectRatio;

        return vec3d(centerX + projectedX + moveX, centerY - projectedY + moveY, z);
    }
};

struct triangle {
    vec3d p1, p2, p3;
    triangle(vec3d p1, vec3d p2, vec3d p3) : p1(p1), p2(p2), p3(p3) {}
};

class Object {
public:
    virtual void generateVertices() = 0;
    virtual void generateIndices() = 0;
    virtual const std::vector<triangle>& getTriangles() const = 0;
    virtual void rotate(float angleX, float angleY, float angleZ) = 0;
    virtual ~Object() = default;
};

class Sphere : public Object {
public:
    Sphere(float radius, int latitudeSteps, int longitudeSteps)
        : radius(radius), latitudeSteps(latitudeSteps), longitudeSteps(longitudeSteps) {
        generateVertices();
        generateIndices();
    }

    void generateVertices() override {
        vertices.clear();
        for (int lat = 0; lat <= latitudeSteps; ++lat) {
            float theta = M_PI * lat / latitudeSteps;
            float sinTheta = sin(theta);
            float cosTheta = cos(theta);

            for (int lon = 0; lon <= longitudeSteps; ++lon) {
                float phi = 2 * M_PI * lon / longitudeSteps;
                float sinPhi = sin(phi);
                float cosPhi = cos(phi);

                float x = radius * sinTheta * cosPhi;
                float y = radius * cosTheta;
                float z = radius * sinTheta * sinPhi;

                vertices.push_back(vec3d(x, y, z));
            }
        }
    }

    void generateIndices() override {
        triangles.clear();
        for (int lat = 0; lat < latitudeSteps; ++lat) {
            for (int lon = 0; lon < longitudeSteps; ++lon) {
                int first = lat * (longitudeSteps + 1) + lon;
                int second = first + longitudeSteps + 1;

                triangles.push_back(triangle(vertices[first], vertices[second], vertices[first + 1]));
                triangles.push_back(triangle(vertices[second], vertices[second + 1], vertices[first + 1]));
            }
        }
    }

    void rotate(float angleX, float angleY, float angleZ) override {
        float cosX = cos(angleX), sinX = sin(angleX);
        float cosY = cos(angleY), sinY = sin(angleY);
        float cosZ = cos(angleZ), sinZ = sin(angleZ);

        for (auto& vertex : vertices) {
            float y = vertex.y * cosX - vertex.z * sinX;
            float z = vertex.y * sinX + vertex.z * cosX;
            vertex.y = y; vertex.z = z;

            float x = vertex.x * cosY + vertex.z * sinY;
            z = -vertex.x * sinY + vertex.z * cosY;
            vertex.x = x; vertex.z = z;

            x = vertex.x * cosZ - vertex.y * sinZ;
            y = vertex.x * sinZ + vertex.y * cosZ;
            vertex.x = x; vertex.y = y;
        }
        generateIndices();
    }

    const std::vector<triangle>& getTriangles() const override {
        return triangles;
    }

private:
    float radius;
    int latitudeSteps;
    int longitudeSteps;
    std::vector<vec3d> vertices;
    std::vector<triangle> triangles;
};

class RenderingEngine {
public:
    RenderingEngine(int width, int height)
        : WIDTH(width), HEIGHT(height), centerX(width / 2), centerY(height / 2), angleX(0), angleY(0), angleZ(0), moveX(0), moveY(0), degree(0), r(400) {}

    ~RenderingEngine() {
        DeleteObject(SelectObject(hdcMem, hOldBitmap));
        DeleteObject(hBitmap);
        DeleteDC(hdcMem);
    }

    void addObject(Object* obj) {
        objects.push_back(obj);
    }

    void Run() {
        WNDCLASS wc = { 0 };
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"PixelWindowClass";

        if (!RegisterClass(&wc)) {
            std::cerr << "Failed to register window class." << std::endl;
            return;
        }

        HWND hwnd = CreateWindowEx(
            0, wc.lpszClassName, L"Pixel Drawing Window", WS_OVERLAPPEDWINDOW,
            CW_USEDEFAULT, CW_USEDEFAULT, WIDTH, HEIGHT,
            NULL, NULL, wc.hInstance, this);

        if (!hwnd) {
            std::cerr << "Failed to create window." << std::endl;
            return;
        }

        ShowWindow(hwnd, SW_SHOWNORMAL);
        UpdateWindow(hwnd);

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        RenderingEngine* engine;

        if (uMsg == WM_CREATE) {
            CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
            engine = (RenderingEngine*)cs->lpCreateParams;
            SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)engine);
        }
        else {
            engine = (RenderingEngine*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        }

        if (engine) {
            return engine->HandleMessage(hwnd, uMsg, wParam, lParam);
        }
        else {
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

private:
    const int WIDTH;
    const int HEIGHT;
    int centerX;
    int centerY;
    float angleX, angleY, angleZ;
    float moveX, moveY;
    float degree;
    int r;
    HDC hdcMem = NULL;
    HBITMAP hBitmap = NULL;
    HBITMAP hOldBitmap = NULL;
    std::vector<Object*> objects;
    void DrawPixel(HDC hdc, int x, int y, COLORREF color) {
        SetPixel(hdc, x, y, color);
    }

    void DrawLine(HDC hdc, int x1, int y1, int x2, int y2, COLORREF color) {
        int dx = abs(x2 - x1), sx = x1 < x2 ? 1 : -1;
        int dy = -abs(y2 - y1), sy = y1 < y2 ? 1 : -1;
        int err = dx + dy, e2;

        while (true) {
            DrawPixel(hdc, x1, y1, color);
            if (x1 == x2 && y1 == y2) break;
            e2 = 2 * err;
            if (e2 >= dy) { err += dy; x1 += sx; }
            if (e2 <= dx) { err += dx; y1 += sy; }
        }
    }

    void DrawTriangle(HDC hdc, vec3d p1, vec3d p2, vec3d p3, COLORREF color) {
        DrawLine(hdc, (int)p1.x, (int)p1.y, (int)p2.x, (int)p2.y, color);
        DrawLine(hdc, (int)p2.x, (int)p2.y, (int)p3.x, (int)p3.y, color);
        DrawLine(hdc, (int)p3.x, (int)p3.y, (int)p1.x, (int)p1.y, color);
    }

    LRESULT HandleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
        case WM_CREATE: {
            HDC hdcWindow = GetDC(hwnd);
            hdcMem = CreateCompatibleDC(hdcWindow);
            hBitmap = CreateCompatibleBitmap(hdcWindow, WIDTH, HEIGHT);
            hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmap);
            ReleaseDC(hwnd, hdcWindow);
            SetTimer(hwnd, 1, 16, NULL);
            break;
        }

        case WM_TIMER: {
            angleX += 0.01f;
            angleY += 0.01f;
            angleZ += 0.01f;

            degree += 3;
            if (degree > 360) degree = 0;

            moveX = r * cos(degree * M_PI / 180.0f);
            moveY = r * sin(degree * M_PI / 180.0f);

            std::thread rotationThread([&]() {
                for (auto obj : objects) {
                    obj->rotate(angleX, angleY, angleZ);
                }
                });
            rotationThread.join();

            InvalidateRect(hwnd, NULL, TRUE);
            break;
        }

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdcWindow = BeginPaint(hwnd, &ps);

            RECT rect;
            GetClientRect(hwnd, &rect);
            FillRect(hdcMem, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));

            for (const auto& obj : objects) {
                const std::vector<triangle>& triangles = obj->getTriangles();
                std::thread drawingThread([&]() {
                    for (const auto& tri : triangles) {
                        vec3d p1 = tri.p1.projectTo2D(centerX, centerY, 8.0f, moveX, moveY);
                        vec3d p2 = tri.p2.projectTo2D(centerX, centerY, 8.0f, moveX, moveY);
                        vec3d p3 = tri.p3.projectTo2D(centerX, centerY, 8.0f, moveX, moveY);
                        DrawTriangle(hdcMem, p1, p2, p3, RGB(0, 0, 255));
                    }
                    });
                drawingThread.join();
            }

            BitBlt(hdcWindow, 0, 0, WIDTH, HEIGHT, hdcMem, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            break;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
        return 0;
    }
};




