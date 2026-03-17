#include <Windows.h>
#include <iostream>
#include <limits>
#include <fstream>
#include <iomanip>
#include <algorithm> // std::clamp



// Logic Constants
constexpr DWORD MAP_OFFSET_HIGH = 0;
constexpr DWORD MAP_OFFSET_LOW = 0;
constexpr BOOL  NO_INHERITANCE = FALSE;

// Physics Constants
constexpr float MAX_G_FORCE = 2.0f;
constexpr int   SERVO_MID_POINT = 90;
constexpr float gForceClip = 2.0f;

HANDLE hSerial;


struct SPageFilePhysics {
    int packetId;
    float gas;
    float brake;
    float fuel;
    int gear;
    int rpms;
    float steerAngle;
    float speedKmh;
    float velocity[3];
    float accG[3]; // [0] = Lateral, [1] = Vertical, [2] = Longitudinal
};

struct gForce {
    float latitude;
    float longitude;
};

struct minMaxGForce {
    float maxLat = std::numeric_limits<float>::lowest();
    float minLat = (std::numeric_limits<float>::max)();
    float maxLon = std::numeric_limits<float>::lowest();
    float minLon = (std::numeric_limits<float>::max)();

    void updateValue(gForce gforce) {
        if (gforce.latitude > maxLat) {
            maxLat = gforce.latitude;
        }
        if (gforce.latitude < minLat) {
            minLat = gforce.latitude;
        }
        if (gforce.longitude > maxLon) {
            maxLon = gforce.longitude;
        }
        if (gforce.longitude < minLon) {
            minLon = gforce.longitude;
        }
    };
};

struct servoDegrees {
    int latDegrees;
    int lonDegrees;
};

servoDegrees mapGToServo(gForce gforce);



int main(int argc, char* argv[]) {

    hSerial = CreateFileA("\\\\.\\COM4", GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) {
        std::cerr << "Error: Could not open Arduino COM port!" << std::endl;
        return 1;
    }

    // Set Port Settings (Baud Rate)
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    if (!GetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error getting state" << std::endl;
    }
    dcbSerialParams.BaudRate = CBR_115200; // Must match Arduino!
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;
    if (!SetCommState(hSerial, &dcbSerialParams)) {
        std::cerr << "Error setting state" << std::endl;
    }

    HANDLE hMapFile = OpenFileMappingA(FILE_MAP_READ, NO_INHERITANCE, "Local\\acpmf_physics");

    if (hMapFile == nullptr) {
        std::cout << "Could not open shared memory. Make sure Assetto Corsa is running\n\n";
        return 1;
    }

	SPageFilePhysics* pf = (SPageFilePhysics*)MapViewOfFile(hMapFile, FILE_MAP_READ, MAP_OFFSET_HIGH, MAP_OFFSET_LOW, sizeof(SPageFilePhysics));

    minMaxGForce minMaxG;
    gForce gforce;
    float smoothedLat = 90.0f;
    float smoothedLon = 90.0f;
    float smoothingFactor = 0.5f; // 0.1 = very smooth/slow, 0.5 = snappy/sharp
    while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
        float latitude = pf->accG[0];
        float longitude = pf->accG[2];
        
        gforce.latitude = latitude;
        gforce.longitude = longitude;
        
        minMaxG.updateValue(gforce);

        servoDegrees servodegrees = mapGToServo(gforce);

        servoDegrees target = mapGToServo(gforce);
        smoothedLat = smoothedLat + (target.latDegrees - smoothedLat) * smoothingFactor;
        smoothedLon = smoothedLon + (target.lonDegrees - smoothedLon) * smoothingFactor;

        // 2. Create the packet [Start, Lat, Lon, End]
        // We cast to unsigned char to ensure they are single bytes
        unsigned char packet[4];
        packet[0] = '<';
        packet[1] = static_cast<unsigned char>(std::round(smoothedLat));
        packet[2] = static_cast<unsigned char>(std::round(smoothedLon));
        packet[3] = '>';

        // 3. Send to Arduino
        DWORD bytesWritten;
        WriteFile(hSerial, packet, 4, &bytesWritten, NULL);
        


        std::cout << "Lat G: " << std::fixed << std::setprecision(2) << gforce.latitude
            << " | Lon G: " << gforce.longitude
            << " | Servo Lat: " << servodegrees.latDegrees
            << " | Servo Lon: " << servodegrees.lonDegrees
            << "                \r" << std::flush;
        Sleep(16);
    }
    std::ofstream outFile("gForce.txt");

    if (outFile.is_open()) {

        outFile << std::fixed << std::setprecision(4);

        outFile << "Max Lat: " << minMaxG.maxLat << "\n";
        outFile << "Min Lat: " << minMaxG.minLat << "\n";
        outFile << "Max Long: " << minMaxG.maxLon << "\n";
        outFile << "Min Long: " << minMaxG.minLon << "\n";

        outFile.close();

        if (outFile.good()) {
            std::cout << "File written successfully." << std::endl;
        }
    }
    
    UnmapViewOfFile(pf);
    CloseHandle(hMapFile);
    CloseHandle(hSerial);



    
    return 0;

}

servoDegrees mapGToServo(gForce gforce) {
    servoDegrees deg;

    // 1. Clamp to your defined physical limits
    float clippedLat = std::clamp(gforce.latitude, -gForceClip, gForceClip);
    float clippedLon = std::clamp(gforce.longitude, -gForceClip, gForceClip);

    // 2. Shift and Scale (Normalized to 0.0 - 180.0)
    // We use .0f to ensure floating point math throughout
    float latRaw = (clippedLat + gForceClip) * (180.0f / (gForceClip * 2.0f));
    float lonRaw = (clippedLon + gForceClip) * (180.0f / (gForceClip * 2.0f));

    // 3. Round to nearest integer for the servo
    deg.latDegrees = static_cast<int>(std::round(latRaw));
    deg.lonDegrees = static_cast<int>(std::round(lonRaw));

    return deg;
}