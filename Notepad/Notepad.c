#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"

// Struct to hold page setup settings (margins and orientation)
typedef struct PageSettings {
	int leftTwips;      // Left margin in twips (1/1440 inch)
	int rightTwips;     // Right margin in twips
	int topTwips;       // Top margin in twips
	int bottomTwips;    // Bottom margin in twips
	BOOL orientation;   // 0 = Portrait, 1 = Landscape
} PageSettings;

// Global instance for page setup settings, initialized to 1 inch margins, portrait
PageSettings g_pageSettings = { 1440, 1440, 1440, 1440, FALSE };

// Global handles and variables for main window, editor control, font, etc.
HWND hMainWindow;     // Handle to main window
HWND hEditor;         // Handle to edit control
HFONT consolas;       // Font handle for Consolas font
int hEditorWidth = 0, hEditorHeight = 0; // Editor control size
wchar_t CURRENT_FILE[MAX_PATH] = L"Untitled"; // Current file name
HMENU hMenu, hFileMenu; // Handles for main menu and file menu

// Function declarations
LRESULT CALLBACK WindowProcedure(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK PageSetupProcedure(HWND, UINT, WPARAM, LPARAM);
void pageSetupDialog(HWND);
void AddMenu(HWND);
void AddControl(HWND);
void open_file(HWND);
void save_file(HWND, BOOL);
const wchar_t* FindFileName(const wchar_t*);

// Entry point for the application
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR args, int nCmdShow) {
	const wchar_t CLASS_NAME[] = L"RENotepad";

	// Define and register window class
	WNDCLASSW wc = { 0 };
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;
	wc.lpfnWndProc = WindowProcedure;

	if (!RegisterClassW(&wc))
		return -1;

	// Create main window
	hMainWindow = CreateWindowW(
		CLASS_NAME,
		CURRENT_FILE,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, hInstance, NULL);

	if (!hMainWindow)
		return -1;

	// Load keyboard accelerators (shortcuts)
	HACCEL hAccel = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDR_ACCELERATOR1));

	// Main message loop
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0)) {
		// If no accelerator or not handled, process normally
		if (!hAccel || !TranslateAcceleratorW(hMainWindow, hAccel, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return 0;
}

// Helper: Extracts file name from full path
const wchar_t* FindFileName(const wchar_t* FILE_PATH) {
	const wchar_t* last = FILE_PATH;
	while (*FILE_PATH) {
		if (*FILE_PATH == L'\\' || *FILE_PATH == L'/')
			last = FILE_PATH + 1;
		FILE_PATH++;
	}
	return last;
}

INT_PTR CALLBACK PageSetupProcedure(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
	case WM_INITDIALOG:
		// load dữ liệu cũ vào edit box
		SetDlgItemInt(hDlg, IDC_EDIT_LEFT, g_pageSettings.leftTwips, TRUE);
		SetDlgItemInt(hDlg, IDC_EDIT_RIGHT, g_pageSettings.rightTwips, TRUE);
		SetDlgItemInt(hDlg, IDC_EDIT_TOP, g_pageSettings.topTwips, TRUE);
		SetDlgItemInt(hDlg, IDC_EDIT_BOTTOM, g_pageSettings.bottomTwips, TRUE);
		CheckRadioButton(hDlg, IDC_RADIO_PORTRAIT, IDC_RADIO_LANDSCAPE,
			g_pageSettings.orientation == 0 ? IDC_RADIO_PORTRAIT : IDC_RADIO_LANDSCAPE);
		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			g_pageSettings.leftTwips = GetDlgItemInt(hDlg, IDC_EDIT_LEFT, NULL, TRUE);
			g_pageSettings.rightTwips = GetDlgItemInt(hDlg, IDC_EDIT_RIGHT, NULL, TRUE);
			g_pageSettings.topTwips = GetDlgItemInt(hDlg, IDC_EDIT_TOP, NULL, TRUE);
			g_pageSettings.bottomTwips = GetDlgItemInt(hDlg, IDC_EDIT_BOTTOM, NULL, TRUE);
			g_pageSettings.orientation =
				(IsDlgButtonChecked(hDlg, IDC_RADIO_LANDSCAPE) == BST_CHECKED) ? 1 : 0;
			EndDialog(hDlg, IDOK);
			return TRUE;

		case IDCANCEL:
			EndDialog(hDlg, IDCANCEL);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

// Show the Page Setup dialog
void pageSetupDialog(HWND hwnd) {
	DialogBoxParamW(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_PAGESETUP), hwnd, PageSetupProcedure, 0);
}

// Open a file and load its contents into the editor
void open_file(HWND hwnd) {
	OPENFILENAMEW ofn = { sizeof(ofn) };
	wchar_t file_name[MAX_PATH] = L"";
	// Setup open file dialog
	ofn.hwndOwner = hwnd;
	ofn.lpstrFile = file_name;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = L"Text Files\0*.txt\0All Files\0*.*\0";
	ofn.nFilterIndex = 1;

	if (!GetOpenFileNameW(&ofn))
		return;

	// Open file for reading (binary mode)
	FILE* file = _wfopen(ofn.lpstrFile, L"rb");
	if (!file) {
		MessageBoxW(hwnd, L"Failed to open file", L"Error", MB_OK);
		return;
	}

	// Get file size
	fseek(file, 0, SEEK_END);
	long bytes = ftell(file);
	rewind(file);

	// Calculate wchar_t count and allocate buffer
	size_t wchar_count = (size_t)bytes / sizeof(wchar_t);
	wchar_t* wdata = (wchar_t*)malloc((wchar_count + 1) * sizeof(wchar_t));
	fread(wdata, sizeof(wchar_t), wchar_count, file);
	fclose(file);
	wdata[wchar_count] = L'\0';

	// Set text in editor control
	SetWindowTextW(hEditor, wdata);
	// Update current file name and window title
	wcscpy_s(CURRENT_FILE, MAX_PATH, ofn.lpstrFile);
	SetWindowTextW(hwnd, FindFileName(CURRENT_FILE));
	free(wdata);
}

// Save the contents of the editor to a file
void save_file(HWND hwnd, BOOL isSaveAs) {
	wchar_t file_name[MAX_PATH] = L"";
	// If Save As or new file, show save dialog
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

	// Open file for writing (binary mode)
	FILE* file = _wfopen(file_name, L"wb");
	if (!file) {
		MessageBoxW(hwnd, L"Cannot write file", L"Error", MB_OK);
		return;
	}

	// Get text from editor and write to file
	int len = GetWindowTextLengthW(hEditor);
	wchar_t* content = (wchar_t*)malloc((len + 1) * sizeof(wchar_t));
	GetWindowTextW(hEditor, content, len + 1);
	fwrite(content, sizeof(wchar_t), len, file);
	fclose(file);
	free(content);

	// Update current file name and window title
	wcscpy_s(CURRENT_FILE, MAX_PATH, file_name);
	SetWindowTextW(hwnd, FindFileName(file_name));
}

// Main window procedure: handles messages/events
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_CREATE:
		// Create menu and editor control
		AddMenu(hwnd);
		AddControl(hwnd);
		break;
	case WM_COMMAND:
		// Handle menu commands
		switch (LOWORD(wParam)) {
		case ID_NEW_FILE:
			// Clear editor and reset file name
			SetWindowTextW(hEditor, L"");
			wcscpy_s(CURRENT_FILE, MAX_PATH, L"Untitled");
			SetWindowTextW(hwnd, CURRENT_FILE);
			break;
		case ID_OPEN_FILE:
			open_file(hwnd);
			break;
		case ID_SAVE_FILE:
			save_file(hwnd, FALSE);
			break;
		case ID_SAVE_AS_FILE:
			save_file(hwnd, TRUE);
			break;
		case ID_PAGE_SETUP_DIALOG:
			pageSetupDialog(hwnd);
			break;
		case ID_EXIT:
			DestroyWindow(hwnd);
			break;
		}
		break;
	case WM_SIZE:
		// Resize editor control to fill window
		hEditorWidth = LOWORD(lParam);
		hEditorHeight = HIWORD(lParam);
		MoveWindow(hEditor, 0, 0, hEditorWidth, hEditorHeight, TRUE);
		break;
	case WM_DESTROY:
		// Cleanup font and exit
		if (consolas) {
			DeleteObject(consolas);
			consolas = NULL;
		}
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
	return 0;
}

// Create the main menu and file menu
void AddMenu(HWND hwnd) {
	hMenu = CreateMenu();
	hFileMenu = CreateMenu();
	AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"File");
	AppendMenuW(hFileMenu, MF_STRING, ID_NEW_FILE, L"New\tCtrl+N");
	AppendMenuW(hFileMenu, MF_STRING, ID_OPEN_FILE, L"Open...\tCtrl+O");
	AppendMenuW(hFileMenu, MF_STRING, ID_SAVE_FILE, L"Save\tCtrl+S");
	AppendMenuW(hFileMenu, MF_STRING, ID_SAVE_AS_FILE, L"Save As...\tCtrl+Shift+S");
	AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hFileMenu, MF_STRING, ID_PAGE_SETUP_DIALOG, L"Page Setup...");
	AppendMenuW(hFileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(hFileMenu, MF_STRING, ID_EXIT, L"Exit\tAlt+F4");
	SetMenu(hwnd, hMenu);
}

// Create the editor control and set its font
void AddControl(HWND hwnd) {
	// Get DPI for font size calculation
	HDC hdc = GetDC(hwnd);
	int logpix = GetDeviceCaps(hdc, LOGPIXELSY);
	ReleaseDC(hwnd, hdc);

	// Create Consolas font, 12pt
	consolas = CreateFontW(-MulDiv(12, logpix, 72), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_OUTLINE_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
		VARIABLE_PITCH | FF_DONTCARE, L"Consolas");

	// Create multiline edit control for text editing
	hEditor = CreateWindowW(L"Edit", L"",
		WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_HSCROLL | ES_WANTRETURN,
		0, 0, hEditorWidth, hEditorHeight, hwnd, NULL, GetModuleHandle(NULL), NULL);

	// Set font for editor control
	SendMessageW(hEditor, WM_SETFONT, (WPARAM)consolas, TRUE);
}
