#include "SimpleSquash.h"

HANDLE hInputFile = INVALID_HANDLE_VALUE;
HANDLE hOutputFile = INVALID_HANDLE_VALUE;

TCHAR pszInputFilePath[MAX_PATH];
TCHAR pszOutputFilePath[MAX_PATH];

std::wstring action;

PBYTE pbInputFileBuffer = NULL;
PBYTE pbOutputFileBuffer = NULL;

SIZE_T InputDataSize, InputBufferSize;

DWORD dwInputFileSize, dwBytesRead, dwBytesWritten;

SIZE_T nOutputFileSize;

COMPRESSOR_HANDLE hCompressor = NULL;
DECOMPRESSOR_HANDLE hDecompressor = NULL;

BOOL deleteOutputFile = FALSE;

void print_usage()
{
    std::cout << "Usage: SimpleSquash.exe <compress|decompress> <input-path> <output-path>" << std::endl;;
}

// main checks arguments and opens file handles, before handing off to the relevant action function.
int _tmain(int argc, _TCHAR* argv[])
{
    LARGE_INTEGER FileSize;

    if (argc != 4)
    {
        std::cout << "Incorrect number of arguments." << std::endl;
        print_usage();
        return 1;
    }

    action = argv[1];

    if (action.compare(L"compress") != 0 && action.compare(L"decompress") != 0)
    {
        std::cout << "Invalid action \"" << action << "\"." << std::endl;
        print_usage();
        return 1;
    }

    if (GetFullPathName(argv[2], MAX_PATH, pszInputFilePath, NULL) == 0)
    {
        std::cerr << "Failed to get full path of file \"" << argv[2] << "\"." << std::endl;
        return 1;
    }

    if (GetFullPathName(argv[3], MAX_PATH, pszOutputFilePath, NULL) == 0)
    {
        std::cerr << "Failed to get full path of file \"" << argv[3] << "\"." << std::endl;
        return 1;
    }

    hInputFile = CreateFile(pszInputFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hInputFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open file at \"" << pszInputFilePath << "\"." << std::endl;
        return 1;
    }

    hOutputFile = CreateFile(pszOutputFilePath, GENERIC_WRITE | DELETE, FILE_SHARE_WRITE, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hOutputFile == INVALID_HANDLE_VALUE)
    {
        std::cerr << "Failed to open file for writing at \"" << pszOutputFilePath << "\"." << std::endl;
        return 1;
    }
    deleteOutputFile = TRUE;

    if (!GetFileSizeEx(hInputFile, &FileSize))
    {
        std::cerr << "Failed to get file size of \"" << pszInputFilePath << "\"." << std::endl;
        return 1;
    }

    dwInputFileSize = FileSize.LowPart;
    pbInputFileBuffer = (PBYTE)malloc(dwInputFileSize);

    if (!pbInputFileBuffer)
    {
        std::cerr << "Failed to allocate memory on the Heap for input file content." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    if (!ReadFile(hInputFile, pbInputFileBuffer, dwInputFileSize, &dwBytesRead, NULL))
    {
        std::cerr << "Failed to read input file into memory." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    if (dwInputFileSize != dwBytesRead)
    {
        std::cerr << "Expected to read " << dwInputFileSize << " bytes but only read " << dwBytesRead << " bytes." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    std::cout << std::endl;
    std::cout << "Input File:" << '\t' << "\"" << pszInputFilePath << "\"." << std::endl;
    std::cout << "Input File Size:" << '\t' << dwBytesRead << " bytes." << std::endl;

    if(action.compare(L"compress") == 0)
    {
        return compress_file();
    }
    else
    {
        std::cout << "Decompressing..." << std::endl;
        return decompress_file();
    }
}

int compress_file()
{
    if (!CreateCompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &hCompressor))
    {
        std::cerr << "Cannot create compressor." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    // Populate relevant file sizes.
    if (!Compress(hCompressor, pbInputFileBuffer, dwInputFileSize, NULL, 0, &nOutputFileSize))
    {
        DWORD ErrorCode = GetLastError();

        if (ErrorCode != ERROR_INSUFFICIENT_BUFFER)
        {
            std::cerr << "Cannot compress data: code = " << ErrorCode << "." << std::endl;
            return cleanup(STATUS_ERROR);
        }

        pbOutputFileBuffer = (PBYTE)malloc(nOutputFileSize);
        if (!pbOutputFileBuffer)
        {
            std::cerr << "Cannot allocate memory for compressed buffer." << std::endl;
            return cleanup(STATUS_ERROR);
        }
    }

    // Perform the actual compression.
    if (!Compress(
        hCompressor,
        pbInputFileBuffer,
        dwInputFileSize,
        pbOutputFileBuffer,
        nOutputFileSize,
        &nOutputFileSize))
    {
        std::cerr << "Cannot compress data: " << GetLastError() << "." << std::endl;
        return cleanup(STATUS_ERROR);
    }
    return write_output_buffer();
}

int decompress_file()
{
    if (!CreateDecompressor(COMPRESS_ALGORITHM_XPRESS_HUFF, NULL, &hDecompressor))
    {
        std::cerr << "Cannot create compressor." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    // Populate relevant file sizes.
    if (!Decompress(hDecompressor, pbInputFileBuffer, dwInputFileSize, NULL, 0, &nOutputFileSize))
    {
        DWORD ErrorCode = GetLastError();

        if (ErrorCode != ERROR_INSUFFICIENT_BUFFER)
        {
            std::cerr << "Cannot decompress data: code = " << ErrorCode << "." << std::endl;
            return cleanup(STATUS_ERROR);
        }

        pbOutputFileBuffer = (PBYTE)malloc(nOutputFileSize);
        if (!pbOutputFileBuffer)
        {
            std::cerr << "Cannot allocate memory for decompressed buffer." << std::endl;
            return cleanup(STATUS_ERROR);
        }
    }

    // Perform the actual decompression.
    if (!Decompress(
        hDecompressor,
        pbInputFileBuffer,
        dwInputFileSize,
        pbOutputFileBuffer,
        nOutputFileSize,
        &nOutputFileSize))
    {
        std::cerr << "Cannot decompress data: " << GetLastError() << "." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    return write_output_buffer();
}

// Writes the buffer to the relevant output file.
int write_output_buffer()
{
    if (!WriteFile(
        hOutputFile,
        pbOutputFileBuffer,
        nOutputFileSize,
        &dwBytesWritten,
        NULL))
    {
        std::cerr << "Cannot write file: " << GetLastError() << "." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    if (dwBytesWritten != nOutputFileSize)
    {
        std::cerr << "Expected to write " << nOutputFileSize << " bytes but only wrote " << dwBytesWritten << " bytes." << std::endl;
        return cleanup(STATUS_ERROR);
    }

    deleteOutputFile = FALSE;
    std::cout << "Operation completed." << std::endl;
    std::cout << "File:" << '\t' << "\"" << pszOutputFilePath << "\"." << std::endl;
    std::cout << "Size:" << '\t' << dwBytesWritten << " bytes." << std::endl;
    return cleanup(STATUS_SUCCESS);
}

int cleanup(int returnCode)
{
    if (hCompressor != NULL)
    {
        CloseCompressor(hCompressor);
    }

    if (pbOutputFileBuffer)
    {
        free(pbOutputFileBuffer);
    }

    if (pbInputFileBuffer)
    {
        free(pbInputFileBuffer);
    }

    if (hInputFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hInputFile);
    }

    if (hOutputFile != INVALID_HANDLE_VALUE)
    {
        if (deleteOutputFile)
        {
            FILE_DISPOSITION_INFO fdi;
            fdi.DeleteFile = TRUE;
            if(SetFileInformationByHandle(
                hOutputFile,
                FileDispositionInfo,
                &fdi,
                sizeof(FILE_DISPOSITION_INFO)))
            {
                std::cerr << "Failed to delete corruped output file." << std::endl;
                returnCode = STATUS_ERROR;
            }
        }
        CloseHandle(hOutputFile);
    }
    return returnCode;
}