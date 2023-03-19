
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define PH 0x90EB90EB
#define LOFEE 0x790E
#define LO 0x78FE
#define EOFFSET 0x6812
#define COFFSET 0x7812

unsigned int Spectrum[13][1024];
unsigned int ESpectrum[13][4096];
unsigned char* pHead, * pData, * pEnd;

//function for find frame head
int FindNextHead() {
    while (pData < pEnd) {
        switch (*(pData + 3)) {
        case 0x90:
            if (*(int*)pData == PH) return 0;
            if (*(int*)(pData + 2) == PH) {
                pData += 2;
                return 0;
            }
            break;
        case 0xeb:
            if (*(int*)(pData + 1) == PH) {
                pData += 1;
                return 0;
            }
            if (*(int*)(pData + 3) == PH) {
                pData += 3;
                return 0;
            }
        }
        pData += 4;
    }
    pData = pEnd;
    return 1;
}


int main(int argc, char* argv[])
{
    HANDLE hFile, hMapFile;
    DWORD dwFileSizeHigh, dwFileSizeLow;
    TCHAR tcFileName[256];
    char cFullPath[256];
    unsigned int iPackageLabel, iPackageLength, iOfficeSet;
    unsigned int iDiscardLength;
    int ChannelID;

    FILE* fResult;

    ULONGLONG start, end;
    unsigned int iPackageNum, iPackageCountO, iPackageCountC;
    unsigned int iSyncroCount, iSyncroCountC;
    unsigned char iPackageID, iPackageIDC, iPackageType;
    int iChannel, iADC;
    char* pf;

    if (argc == 3) {
        strcpy_s(cFullPath, strlen(argv[1]) + 1, argv[1]);
        ChannelID = atoi(argv[2]);
        if (ChannelID < 0 || ChannelID > 12) ChannelID = 0;
    }
    else {
        strcpy_s(cFullPath, 25, "d:\\data\\testFEE_FEE0.dat");
        ChannelID = 1;
    }
    iDiscardLength = 2;

    printf("Processing Channel is %d\n", ChannelID);

    start = GetTickCount64();

    //open map of view for the data file
    MultiByteToWideChar(CP_ACP, 0, cFullPath, strlen(cFullPath) + 1, tcFileName, sizeof(tcFileName) / sizeof(tcFileName[0]));
    hFile = CreateFile(tcFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        printf("ERROR!!! hFile open failed: %08X\n", GetLastError());
        return 4;
    }
    dwFileSizeLow = GetFileSize(hFile, &dwFileSizeHigh);
    if (dwFileSizeLow == 0 && dwFileSizeHigh == 0) {
        printf("ERROR!!! hFile file size is 0\n");
        CloseHandle(hFile);
        return 4;
    }
    printf("File Length is: 0x%08X %08X\n", dwFileSizeHigh, dwFileSizeLow);
    hMapFile = CreateFileMapping(hFile, NULL, PAGE_READONLY, dwFileSizeHigh, dwFileSizeLow, NULL);
    if (hMapFile == NULL) {
        printf("ERROR!!! hMapFile is NULL: last error: %08X\n", GetLastError());
        CloseHandle(hFile);
        return 2;
    }
    pHead = (unsigned char*)MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
    if (pHead == NULL) {
        printf("ERROR!!! lpMapAddress is NULL: last error: %d\n", GetLastError());
        return 3;
    }
    pData = pHead;
    pEnd = pHead + ((LONGLONG)(dwFileSizeHigh) << 32) + dwFileSizeLow;

    iPackageIDC = 0xff;
    iSyncroCountC = 0x0fff;
    iPackageCountC = 0;
    iPackageCountO = 0;
    iPackageNum = 0;

    while (pData < pEnd) {
        iPackageLabel = *(unsigned int*)pData;
        if (iPackageLabel != PH) {
            printf("Package Head Label Error at: 0x%Ix\n", pData - pHead);
            pData += LOFEE;
            FindNextHead();
            continue;
        }

        //package number check
        if (iPackageNum < iDiscardLength) {
            printf("Discard package of number: %d at 0x%Ix\n", iPackageNum, pData - pHead);
            pData += LOFEE;
            FindNextHead();
            iPackageNum++;
            continue;
        }

        //check package ID
        iPackageID = *(pData + 10);
        iPackageType = iPackageID & 0x0f;
        iPackageID >>= 4;
        if (iPackageID == iPackageIDC || iPackageIDC == 0xff) iPackageIDC = iPackageID;
        else printf("Package ID Errot at: 0x%IX\n", pData - pHead);
        //check package type
        if (iPackageType == 5) iPackageCountO++;
        else {
            printf("Package Type is not obs at: 0x%IX\n", pData - pHead);
            pData += LOFEE;
            FindNextHead();
            continue;
        }
        //check package length
        iPackageLength = 0;
        for (int i = 12; i < 16; i++) {
            iPackageLength <<= 8;
            iPackageLength += *(pData + i);
        }
        if (iPackageLength != LO) {
            printf("Package Length Error at: 0x%IX\n", pData - pHead);
            pData += LOFEE;
            FindNextHead();
            continue;
        }
        //check synchronize number
        iSyncroCount = *(pData + 16) * 256 + *(pData + 17);
        if (iSyncroCount != iSyncroCountC + 1 && iSyncroCountC != 0x0fff) {
            printf("Discontinuous Synchronize number at: 0x%IX\n", pData - pHead);
        }
        iSyncroCountC = iSyncroCount;


        //Process observation package
        if (iPackageType == 5) {
            //Process spectrum data
            for (int i = 0; i < 13; i++) {
                iOfficeSet = i * 2048 + 18;
                for (int j = 0; j < 1024; j++) {
                    Spectrum[i][j] += *(pData + iOfficeSet + 2 * j) * 256 + *(pData + iOfficeSet + 2 * j + 1);
                }
            }
            //Process event data
            for (int i = 0; i < 1024; i++) {
                if (*(pData + EOFFSET + 4 * i) == 0xff) break;
                iChannel = *(pData + EOFFSET + 4 * i);
                iADC = (iChannel & 0xf) * 256 + *(pData + EOFFSET + 4 * i + 1);
                iChannel >>= 4;
                ESpectrum[iChannel][iADC] += 1;
            }

        }

        iPackageNum++;
        pData += LOFEE;
    }

    //In the end close all handles for map view of file
    UnmapViewOfFile(pData);
    CloseHandle(hMapFile);
    CloseHandle(hFile);

    end = GetTickCount64();
    printf("\nTotal process time is: %I64dms\n\n", end - start);

    printf("Total Observation Package is: %d, Total Calibration Package is: %d\n\n", iPackageCountO, iPackageCountC);

    pf = strrchr(cFullPath, '.');
    strcpy_s(pf, 9, "_SPE.csv");
    fResult = fopen(cFullPath, "w");
    fprintf_s(fResult, "channel %d\n", ChannelID);
    for (int i = 0; i < 1024; i++)fprintf_s(fResult, "%d\n", Spectrum[ChannelID][i]);
    fclose(fResult);

    pf = strrchr(cFullPath, '.');
    strcpy_s(pf, 9, "ESPE.csv");
    fResult = fopen(cFullPath, "w");
    fprintf_s(fResult, "channel %d\n", ChannelID);
    for (int i = 0; i < 4096; i++)fprintf_s(fResult, "%d\n", ESpectrum[ChannelID][i]);
    fclose(fResult);

	return 0;
}

