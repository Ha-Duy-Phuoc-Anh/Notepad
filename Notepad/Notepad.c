#include <windows.h>
#include <commdlg.h>  // For common dialog boxes (Open, Save, Print, etc.)
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"

// Define PSD_ENABLEPRINTER if not already defined
#ifndef PSD_ENABLEPRINTER
#define PSD_ENABLEPRINTER 0x00000800  // Enable printer selection in Page Setup
#endif

// Structure to store page layout settings
typedef struct PageSettings {
    wchar_t* paper;    // Paper size name (e.g., "Letter", "A4")
    wchar_t* source;   // Paper source (e.g., "Tray 1", "Manual Feed")
    int leftTwips, rightTwips, topTwips, bottomTwips;  // Margins in twips (1/1440 inch)
    int orientation;   // 0=Portrait, 1=Landscape
} PageSettings;

// Global variables for page setup and printing
PageSettings g_pageSettings;        // Stores current page settings
HGLOBAL g_hDevMode = NULL;          // Printer device settings handle
HGLOBAL g_hDevNames = NULL;         // Printer device name handle

// Global window handles and UI elements
HWND hMainWindow, hEditor;                      // Main window and editor control handles
HFONT editorFont;                               // Font used in editor
wchar_t CURRENT_FILE[MAX_PATH] = L"Untitled";   // Current file path/name
HMENU hMenu, hFileMenu, hEditMenu;              // Menu handles

// Function declarations
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
void pageSetupDialog(HWND);
void printDialog(HWND);
void AddMenu(HWND);
void AddControl(HWND);
void open_file(HWND);
void save_file(HWND, BOOL);
const wchar_t* FindFileName(const wchar_t*);

// Application entry point
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR args, int nCmdShow) {
    // Initialize default page settings (1 inch margins, portrait orientation)
    g_pageSettings.leftTwips = g_pageSettings.rightTwips = g_pageSettings.topTwips = g_pageSettings.bottomTwips = 1440;
    g_pageSettings.orientation = 0;  // portrait
    g_pageSettings.paper = _wcsdup(L"Letter");
    g_pageSettings.source = NULL;

    const wchar_t CLASS_NAME[] = L"RENotepad";

    // Register window class
    WNDCLASSW wc = { 0 };
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.lpfnWndProc = WindowProcedure;
    if (!RegisterClassW(&wc))
        return -1;

    // Create main window
    hMainWindow = CreateWindowW(CLASS_NAME, CURRENT_FILE, WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
        NULL, NULL, hInstance, NULL);
    if (!hMainWindow)
        return -1;

    // Message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

// Main window procedure - handles window messages
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
    case WM_CREATE:  // Window creation
        AddMenu(hwnd);
        AddControl(hwnd);
        break;

    case WM_COMMAND:  // Menu and control commands
        switch (LOWORD(wParam)) {
        case ID_NEW_FILE:  // New file command
            SetWindowTextW(hEditor, L"");
            wcscpy_s(CURRENT_FILE, MAX_PATH, L"Untitled");
            SetWindowTextW(hwnd, CURRENT_FILE);
            break;
        case ID_OPEN_FILE:  // Open file command
            open_file(hwnd);
            break;
        case ID_SAVE_FILE:  // Save file command
            save_file(hwnd, FALSE);
            break;
        case ID_SAVE_AS_FILE:  // Save As command
            save_file(hwnd, TRUE);
            break;
        case ID_PAGE_SETUP_DIALOG:  // Page Setup dialog
            pageSetupDialog(hwnd);
            break;
        case ID_PRINT_DIALOG:  // Print dialog
            printDialog(hwnd);
            break;
        case ID_EDIT_UNDO:
			SendMessageW(hEditor, EM_UNDO, 0, 0);
			break;
        case ID_EXIT:  // Exit application
            DestroyWindow(hwnd);
            break;
        }
        break;

    case WM_INITMENUPOPUP: {
		UINT state = (SendMessageW(hEditor, EM_CANUNDO, 0, 0)) ? MF_ENABLED : MF_GRAYED;
		EnableMenuItem(hMenu, ID_EDIT_UNDO, state);
    }

    case WM_SIZE:  // Window resize
        if (hEditor) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            MoveWindow(hEditor, 0, 0, rc.right - rc.left, rc.bottom - rc.top, TRUE);
        }
        break;

    case WM_DESTROY:  // Window destruction - cleanup
        if (editorFont)
            DeleteObject(editorFont);
        if (g_hDevMode)
            GlobalFree(g_hDevMode);
        if (g_hDevNames)
            GlobalFree(g_hDevNames);
        if (g_pageSettings.paper)
            free(g_pageSettings.paper);
        if (g_pageSettings.source)
            free(g_pageSettings.source);
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Display Page Setup dialog and handle user input
void pageSetupDialog(HWND hwnd) {
    PAGESETUPDLGW psd = { 0 };
    psd.lStructSize = sizeof(psd);
    psd.hwndOwner = hwnd;
    psd.Flags = PSD_MARGINS | PSD_DEFAULTMINMARGINS | PSD_ENABLEPRINTER;

    // Use existing printer settings if available
    psd.hDevMode = g_hDevMode;
    psd.hDevNames = g_hDevNames;

    // Convert margins from twips to thousandths of inches
    psd.rtMargin.left = g_pageSettings.leftTwips * 1000 / 1440;
    psd.rtMargin.top = g_pageSettings.topTwips * 1000 / 1440;
    psd.rtMargin.right = g_pageSettings.rightTwips * 1000 / 1440;
    psd.rtMargin.bottom = g_pageSettings.bottomTwips * 1000 / 1440;

    if (PageSetupDlgW(&psd)) {
        // Convert margins back to twips
        g_pageSettings.leftTwips = psd.rtMargin.left * 1440 / 1000;
        g_pageSettings.topTwips = psd.rtMargin.top * 1440 / 1000;
        g_pageSettings.rightTwips = psd.rtMargin.right * 1440 / 1000;
        g_pageSettings.bottomTwips = psd.rtMargin.bottom * 1440 / 1000;

        // Get orientation from device mode
        if (psd.hDevMode) {
            DEVMODEW* dm = (DEVMODEW*)GlobalLock(psd.hDevMode);
            if (dm) {
                g_pageSettings.orientation = (dm->dmOrientation == DMORIENT_LANDSCAPE);
                GlobalUnlock(psd.hDevMode);
            }
        }

        // Update stored printer settings
        if (g_hDevMode)
            GlobalFree(g_hDevMode);
        if (g_hDevNames)
            GlobalFree(g_hDevNames);
        g_hDevMode = psd.hDevMode;
        g_hDevNames = psd.hDevNames;
        psd.hDevMode = psd.hDevNames = NULL;
    }
}

// Handle printing functionality
void printDialog(HWND hwnd) {
    PRINTDLGW pd = { 0 };
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = hwnd;
    pd.Flags = PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE | PD_NOPAGENUMS | PD_NOSELECTION;

    // Use existing printer settings
    pd.hDevMode = g_hDevMode;
    pd.hDevNames = g_hDevNames;

    if (PrintDlgW(&pd)) {
        // Apply page orientation from settings
        if (pd.hDevMode) {
            DEVMODEW* dm = (DEVMODEW*)GlobalLock(pd.hDevMode);
            if (dm) {
                dm->dmOrientation = g_pageSettings.orientation ? DMORIENT_LANDSCAPE : DMORIENT_PORTRAIT;
                dm->dmFields |= DM_ORIENTATION;
                GlobalUnlock(pd.hDevMode);
            }
        }

        // Start printing
        DOCINFO di = { sizeof(DOCINFO), FindFileName(CURRENT_FILE) };
        if (StartDocW(pd.hDC, &di) > 0) {
            // Get text content
            int len = GetWindowTextLengthW(hEditor);
            wchar_t* content = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
            GetWindowTextW(hEditor, content, len + 1);

            // Set up printer font and page layout
            int dpiX = GetDeviceCaps(pd.hDC, LOGPIXELSX);
            int dpiY = GetDeviceCaps(pd.hDC, LOGPIXELSY);

            // Create printer font (Consolas, 12pt)
            HFONT printerFont = CreateFontW(-MulDiv(12, dpiY, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                VARIABLE_PITCH | FF_DONTCARE, L"Consolas");
            HFONT oldFont = (HFONT)SelectObject(pd.hDC, printerFont);

            // Calculate page margins in device units
            RECT rcPage;
            rcPage.left = MulDiv(g_pageSettings.leftTwips, dpiX, 1440);
            rcPage.top = MulDiv(g_pageSettings.topTwips, dpiY, 1440);
            rcPage.right = GetDeviceCaps(pd.hDC, HORZRES) - MulDiv(g_pageSettings.rightTwips, dpiX, 1440);
            rcPage.bottom = GetDeviceCaps(pd.hDC, VERTRES) - MulDiv(g_pageSettings.bottomTwips, dpiY, 1440);

            // Print text page by page
            int printed = 0;
            while (printed < len) {
                StartPage(pd.hDC);
                RECT rcDraw = rcPage;
                DRAWTEXTPARAMS dtp = { sizeof(DRAWTEXTPARAMS) };
                DrawTextExW(pd.hDC, content + printed, len - printed, &rcDraw,
                          DT_LEFT | DT_TOP | DT_WORDBREAK | DT_NOPREFIX | DT_EDITCONTROL, &dtp);
                EndPage(pd.hDC);
                if (dtp.uiLengthDrawn == 0)
                    break;
                printed += dtp.uiLengthDrawn;
            }

            // Cleanup printing resources
            SelectObject(pd.hDC, oldFont);
            DeleteObject(printerFont);
            free(content);
            EndDoc(pd.hDC);
        }

        DeleteDC(pd.hDC);

        // Update stored printer settings
        if (g_hDevMode)
            GlobalFree(g_hDevMode);
        if (g_hDevNames)
            GlobalFree(g_hDevNames);
        g_hDevMode = pd.hDevMode;
        g_hDevNames = pd.hDevNames;
        pd.hDevMode = pd.hDevNames = NULL;
    }
}

// Extract filename from full path
const wchar_t* FindFileName(const wchar_t* path) {
    const wchar_t* last = path;
    while (*path) {
        if (*path == '\\' || *path == '/')
            last = path + 1;
        path++;
    }
    return last;
}

// Open and read a file into the editor
void open_file(HWND hwnd) {
    OPENFILENAMEW ofn = { sizeof(ofn) };
    wchar_t file_name[MAX_PATH] = L"";
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = file_name;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
    ofn.nFilterIndex = 1;

    if (!GetOpenFileNameW(&ofn))
        return;

    // Open file in binary mode
    FILE* file = _wfopen(ofn.lpstrFile, L"rb");
    if (!file) {
        MessageBoxW(hwnd, L"Failed to open file", L"Error", MB_OK);
        return;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long bytes = ftell(file);
    rewind(file);

    // Read file content
    char* buffer = (char*)malloc(bytes + 1);
    if (!buffer) {
        fclose(file);
        MessageBoxW(hwnd, L"Memory allocation failed", L"Error", MB_OK);
        return;
    }
    fread(buffer, 1, bytes, file);
    fclose(file);
    buffer[bytes] = '\0';

    // Handle UTF-8 BOM if present
    int offset = 0;
    if (bytes >= 3 &&
        (unsigned char)buffer[0] == 0xEF &&
        (unsigned char)buffer[1] == 0xBB &&
        (unsigned char)buffer[2] == 0xBF) {
        offset = 3;  // Skip BOM
    }

    // Convert UTF-8 to wide chars
    int wchar_count = MultiByteToWideChar(CP_UTF8, 0, buffer + offset, -1, NULL, 0);
    wchar_t* wdata = (wchar_t*)malloc(wchar_count * sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, buffer + offset, -1, wdata, wchar_count);
    free(buffer);

    // Update editor and window title
    SetWindowTextW(hEditor, wdata);
    wcscpy_s(CURRENT_FILE, MAX_PATH, ofn.lpstrFile);
    SetWindowTextW(hwnd, FindFileName(CURRENT_FILE));
    free(wdata);
}

// Save editor content to a file
void save_file(HWND hwnd, BOOL isSaveAs) {
    wchar_t file_name[MAX_PATH] = L"";
    // Show Save As dialog if requested or for new files
    if (isSaveAs || wcscmp(CURRENT_FILE, L"Untitled") == 0) {
        OPENFILENAMEW ofn = { sizeof(ofn) };
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = file_name;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
        ofn.Flags = OFN_EXPLORER | OFN_OVERWRITEPROMPT;
        if (!GetSaveFileNameW(&ofn))
            return;
    }
    else {
        wcscpy_s(file_name, MAX_PATH, CURRENT_FILE);
    }

    // Open file for writing in binary mode
    FILE* file = _wfopen(file_name, L"wb");
    if (!file) {
        MessageBoxW(hwnd, L"Cannot write file", L"Error", MB_OK);
        return;
    }

    // Get text from editor
    int len = GetWindowTextLengthW(hEditor);
    wchar_t* content = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
    GetWindowTextW(hEditor, content, len + 1);

    // Convert to UTF-8
    int bytes = WideCharToMultiByte(CP_UTF8, 0, content, -1, NULL, 0, NULL, NULL);
    char* utf8 = (char*)malloc(bytes);
    WideCharToMultiByte(CP_UTF8, 0, content, -1, utf8, bytes, NULL, NULL);

    // Write UTF-8 BOM
    unsigned char bom[] = { 0xEF, 0xBB, 0xBF };
    fwrite(bom, 1, 3, file);

    // Write content
    fwrite(utf8, 1, bytes - 1, file);
    fclose(file);

    free(utf8);
    free(content);

    // Update window title
    wcscpy_s(CURRENT_FILE, MAX_PATH, file_name);
    SetWindowTextW(hwnd, FindFileName(file_name));
}

// Create and set up application menu
void AddMenu(HWND hwnd) {
    hMenu = CreateMenu();
    hFileMenu = CreateMenu();
	hEditMenu = CreateMenu();

    // Menu
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"Edit");

    // File menu
    AppendMenuW(hFileMenu, MF_STRING, ID_NEW_FILE, L"New\tCtrl+N");
    AppendMenuW(hFileMenu, MF_STRING, ID_OPEN_FILE, L"Open...\tCtrl+O");
    AppendMenuW(hFileMenu, MF_STRING, ID_SAVE_FILE, L"Save\tCtrl+S");
    AppendMenuW(hFileMenu, MF_STRING, ID_SAVE_AS_FILE, L"Save As...\tCtrl+Shift+S");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFileMenu, MF_STRING, ID_PAGE_SETUP_DIALOG, L"Page Setup...");
    AppendMenuW(hFileMenu, MF_STRING, ID_PRINT_DIALOG, L"Print...\tCtrl+P");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hFileMenu, MF_STRING, ID_EXIT, L"Exit");

    // Edit Menu
	AppendMenuW(hEditMenu, MF_STRING, ID_EDIT_UNDO, L"Undo\tCtrl+Z");

    SetMenu(hwnd, hMenu);
}

// Create and set up editor control
void AddControl(HWND hwnd) {
    // Calculate font size based on system DPI
    HDC hdc = GetDC(hwnd);
    int logpix = GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(hwnd, hdc);

    // Create Consolas font (12pt)
    editorFont = CreateFontW(-MulDiv(12, logpix, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
        VARIABLE_PITCH | FF_DONTCARE, L"Consolas");

    // Get initial window client area size
    RECT rc;
    GetClientRect(hwnd, &rc);

    // Create multiline edit control
    hEditor = CreateWindowW(L"Edit", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | ES_WANTRETURN,
        0, 0, rc.right - rc.left, rc.bottom - rc.top,
        hwnd, NULL, GetModuleHandle(NULL), NULL);

    // Set editor font
    SendMessageW(hEditor, WM_SETFONT, (WPARAM)editorFont, TRUE);
}
