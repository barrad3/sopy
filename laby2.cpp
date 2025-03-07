#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <cstdlib>
#include <ctime>

using namespace std;

// Identyfikatory timerów i własny komunikat dla kliknięcia w pole
#define TIMER_SEQUENCE 1
#define TIMER_FLASH 2
#define WM_CELL_CLICK (WM_USER + 1)

// Możliwe stany gry
enum GameState {
    STATE_WAITING,       // Oczekiwanie na rozpoczęcie (ESC)
    STATE_SHOW_SEQUENCE, // Wyświetlanie sekwencji ("Zapamiętaj!")
    STATE_GUESSING,      // Oczekiwanie na kliknięcia gracza ("Zgadnij!")
    STATE_FLASH,         // Przejściowy stan – miganie poprawnego kliknięcia
    STATE_GAME_OVER      // Błędny wybór – gra się kończy
};

// Globalne zmienne
HINSTANCE hInst;
int g_gridSize = 3;                     // Domyślny rozmiar planszy (3x3), modyfikowany z argumentu
vector<HWND> g_cells;                   // Uchwyty do okien-komórek planszy
vector<int> g_sequence;                 // Sekwencja indeksów komórek do zapamiętania
int g_seqIndex = 0;                     // Aktualny indeks przy wyświetlaniu sekwencji
bool g_seqHighlighted = false;          // Flaga: czy aktualna komórka jest podświetlona przy animacji sekwencji
int g_guessIndex = 0;                   // Numer kolejnej komórki, którą gracz ma kliknąć
int g_score = 0;                        // Najlepszy wynik gracza (ilość poprawnie powtórzonych pól)
int g_currentHighlightedCell = -1;      // Indeks komórki aktualnie podświetlonej (-1 – brak)
GameState g_gameState = STATE_WAITING;  // Aktualny stan gry
bool g_finalFlash = false;              // Flaga – czy mignięcie dotyczy ostatniej komórki sekwencji

// Pędzle do rysowania
HBRUSH hbrBackground = NULL;     // Tło głównego okna (#FFFDD0)
HBRUSH hbrCellNormal = NULL;     // Kolor pól planszy (#7C0A02)
HBRUSH hbrCellHighlight = NULL;  // Kolor podświetlenia – użyty przy miganiu (tutaj biały)

// Funkcje pomocnicze
void UpdateWindowTitle(HWND hwnd)
{
    wstring title;
    wstringstream ss;
    ss << L"Score: " << g_score << L", ";
    switch (g_gameState)
    {
    case STATE_WAITING:
        ss << L"Naciśnij ESC, aby rozpocząć!";
        break;
    case STATE_SHOW_SEQUENCE:
        ss << L"Zapamiętaj!";
        break;
    case STATE_GUESSING:
    case STATE_FLASH:
        ss << L"Zgadnij!";
        break;
    case STATE_GAME_OVER:
        ss << L"Błędnie! ESC aby zrestartować!";
        break;
    }
    title = ss.str();
    SetWindowText(hwnd, title.c_str());
}

void InvalidateAllCells()
{
    for (HWND cell : g_cells)
    {
        InvalidateRect(cell, NULL, TRUE);
    }
}

void StartSequenceDisplay(HWND hwnd)
{
    g_seqIndex = 0;
    g_seqHighlighted = false;
    g_guessIndex = 0;
    g_gameState = STATE_SHOW_SEQUENCE;
    UpdateWindowTitle(hwnd);
    SetTimer(hwnd, TIMER_SEQUENCE, 1000, NULL);
}

void StartNewGame(HWND hwnd)
{
    // Resetujemy stan gry i sekwencję
    g_sequence.clear();
    g_score = 0;
    g_guessIndex = 0;
    g_seqIndex = 0;
    g_seqHighlighted = false;
    g_currentHighlightedCell = -1;
    g_gameState = STATE_SHOW_SEQUENCE;

    // Dodajemy pierwszy losowy element do sekwencji
    int totalCells = g_gridSize * g_gridSize;
    int randomCell = rand() % totalCells;
    g_sequence.push_back(randomCell);
    UpdateWindowTitle(hwnd);
    SetTimer(hwnd, TIMER_SEQUENCE, 1000, NULL);
}

// Procedura okna głównego
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // Tworzymy okna-komórki w układzie siatki NxN (każde 100x100px)
        int totalCells = g_gridSize * g_gridSize;
        g_cells.reserve(totalCells);
        for (int row = 0; row < g_gridSize; row++)
        {
            for (int col = 0; col < g_gridSize; col++)
            {
                int cellIndex = row * g_gridSize + col;
                int x = col * 100;
                int y = row * 100;
                HWND hCell = CreateWindowEx(0, L"MemoryGameCell", NULL,
                    WS_CHILD | WS_VISIBLE,
                    x, y, 100, 100, hwnd, NULL, hInst, (LPVOID)cellIndex);
                if (hCell)
                    g_cells.push_back(hCell);
            }
        }
        UpdateWindowTitle(hwnd);
        srand((unsigned int)time(NULL)); // inicjujemy generator liczb losowych
        break;
    }
    case WM_KEYDOWN:
    {
        if (wParam == VK_ESCAPE)
        {
            if (g_gameState == STATE_WAITING || g_gameState == STATE_GAME_OVER)
            {
                StartNewGame(hwnd);
            }
        }
        break;
    }
    case WM_TIMER:
    {
        if (wParam == TIMER_SEQUENCE)
        {
            if (g_gameState == STATE_SHOW_SEQUENCE)
            {
                if (!g_seqHighlighted)
                {
                    // Podświetlamy aktualną komórkę z sekwencji
                    g_currentHighlightedCell = g_sequence[g_seqIndex];
                    g_seqHighlighted = true;
                    InvalidateAllCells();
                }
                else
                {
                    // Wyłączamy podświetlenie, przechodzimy do następnego elementu
                    g_currentHighlightedCell = -1;
                    InvalidateAllCells();
                    g_seqHighlighted = false;
                    g_seqIndex++;
                    if (g_seqIndex >= (int)g_sequence.size())
                    {
                        // Koniec animacji – przechodzimy do etapu zgadywania
                        KillTimer(hwnd, TIMER_SEQUENCE);
                        g_gameState = STATE_GUESSING;
                        UpdateWindowTitle(hwnd);
                    }
                }
            }
        }
        else if (wParam == TIMER_FLASH)
        {
            // Zakończenie migania po kliknięciu
            KillTimer(hwnd, TIMER_FLASH);
            g_currentHighlightedCell = -1;
            InvalidateAllCells();
            if (g_finalFlash)
            {
                // Jeśli ostatnia komórka sekwencji została poprawnie kliknięta,
                // dodajemy nowy losowy element, aktualizujemy wynik i ponownie pokazujemy sekwencję
                int totalCells = g_gridSize * g_gridSize;
                int randomCell = rand() % totalCells;
                g_sequence.push_back(randomCell);
                g_score = g_sequence.size() - 1; // wynik to długość sekwencji (opcjonalnie można dostosować)
                StartSequenceDisplay(hwnd);
            }
            else
            {
                // Jeśli nie była to ostatnia komórka – przechodzimy do zgadywania kolejnego pola
                g_gameState = STATE_GUESSING;
                g_guessIndex++; // następna komórka
                UpdateWindowTitle(hwnd);
            }
        }
        break;
    }
    case WM_CELL_CLICK:
    {
        if (g_gameState == STATE_GUESSING)
        {
            int clickedIndex = (int)wParam;
            if (clickedIndex == g_sequence[g_guessIndex])
            {
                // Poprawne kliknięcie – migamy klikniętą komórkę
                g_currentHighlightedCell = clickedIndex;
                InvalidateAllCells();
                g_gameState = STATE_FLASH;
                if (g_guessIndex == (int)g_sequence.size() - 1)
                {
                    // Jeśli to ostatni element sekwencji – migamy przez 1000 ms
                    g_finalFlash = true;
                    SetTimer(hwnd, TIMER_FLASH, 1000, NULL);
                }
                else
                {
                    // W przeciwnym razie krótkie miganie (300 ms)
                    g_finalFlash = false;
                    SetTimer(hwnd, TIMER_FLASH, 300, NULL);
                }
            }
            else
            {
                // Kliknięto złą komórkę – gra kończy się
                g_gameState = STATE_GAME_OVER;
                UpdateWindowTitle(hwnd);
            }
        }
        break;
    }
    case WM_DESTROY:
        KillTimer(hwnd, TIMER_SEQUENCE);
        KillTimer(hwnd, TIMER_FLASH);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Procedura dla okien-komórek planszy
LRESULT CALLBACK CellWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        // W parametrze lpCreateParams przekazujemy indeks komórki – zapisujemy go w GWLP_USERDATA
        LPCREATESTRUCT pcs = (LPCREATESTRUCT)lParam;
        int cellIndex = (int)pcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cellIndex);
        break;
    }
    case WM_LBUTTONUP:
    {
        int cellIndex = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        // Informujemy okno rodzica, że kliknięto daną komórkę
        PostMessage(GetParent(hwnd), WM_CELL_CLICK, (WPARAM)cellIndex, 0);
        break;
    }
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);
        // Rysujemy kwadrat o wymiarach 90x90px, z marginesem 5px od krawędzi
        RECT innerRect = { 5, 5, rect.right - 5, rect.bottom - 5 };
        int cellIndex = (int)GetWindowLongPtr(hwnd, GWLP_USERDATA);
        HBRUSH brush = (cellIndex == g_currentHighlightedCell) ? hbrCellHighlight : hbrCellNormal;
        FillRect(hdc, &innerRect, brush);
        EndPaint(hwnd, &ps);
        break;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

// Funkcja WinMain – punkt wejścia aplikacji
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR    lpCmdLine,
                      _In_ int       nCmdShow)
{
    hInst = hInstance;

    // Odczytujemy argument z linii poleceń: rozmiar planszy (od 3 do 10), domyślnie 3
    int n = 3;
    int argc;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argc > 1)
    {
        n = _wtoi(argv[1]);
        if (n < 3 || n > 10)
            n = 3;
    }
    g_gridSize = n;
    LocalFree(argv);

    // Rejestrujemy klasę okna głównego
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wcex.lpfnWndProc = MainWndProc;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    hbrBackground = CreateSolidBrush(RGB(255, 253, 208)); // Tło: #FFFDD0
    wcex.hbrBackground = hbrBackground;
    wcex.lpszClassName = L"MemoryGameMainWindow";
    wcex.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassEx(&wcex))
        return 1;

    // Rejestrujemy klasę dla pól planszy
    WNDCLASSEX wcexCell = { 0 };
    wcexCell.cbSize = sizeof(WNDCLASSEX);
    wcexCell.style = CS_HREDRAW | CS_VREDRAW;
    wcexCell.lpfnWndProc = CellWndProc;
    wcexCell.hInstance = hInst;
    wcexCell.hCursor = LoadCursor(NULL, IDC_ARROW);
    hbrCellNormal = CreateSolidBrush(RGB(124, 10, 2)); // Kolor pól: #7C0A02
    wcexCell.hbrBackground = hbrCellNormal;
    wcexCell.lpszClassName = L"MemoryGameCell";
    if (!RegisterClassEx(&wcexCell))
        return 1;

    // Pędzel dla podświetlonej komórki (np. biały)
    hbrCellHighlight = CreateSolidBrush(RGB(255, 255, 255));

    // Obliczamy rozmiar głównego okna – plansza ma wymiar: NxN pól po 100x100px (w tym margines)
    int clientWidth = g_gridSize * 100;
    int clientHeight = g_gridSize * 100;
    RECT rc = { 0, 0, clientWidth, clientHeight };
    // Styl okna: brak możliwości zmiany rozmiaru (bez WS_THICKFRAME/WS_MAXIMIZEBOX), lecz z możliwością minimalizacji
    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
    AdjustWindowRect(&rc, dwStyle, FALSE);
    int windowWidth = rc.right - rc.left;
    int windowHeight = rc.bottom - rc.top;

    // Ustalamy pozycję – okno na środku ekranu
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;

    HWND hwndMain = CreateWindowEx(0, L"MemoryGameMainWindow",
        L"Score: 0, Naciśnij ESC, aby rozpocząć!",
        dwStyle,
        x, y, windowWidth, windowHeight,
        NULL, NULL, hInst, NULL);
    if (!hwndMain)
        return 1;

    ShowWindow(hwndMain, nCmdShow);
    UpdateWindow(hwndMain);

    // Pętla komunikatów
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Czyszczenie zasobów
    DeleteObject(hbrBackground);
    DeleteObject(hbrCellNormal);
    DeleteObject(hbrCellHighlight);

    return (int)msg.wParam;
}
