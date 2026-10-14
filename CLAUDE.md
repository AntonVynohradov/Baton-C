# intercom: kontekst projektu

Biblioteka C99 (static lib) do deterministycznej komunikacji master/slave po jednoprzewodowym UART (half-duplex, domyślnie 9600 bit/s) dla systemów embedded. Podłączana do projektów jako git submodule.

## Konwencje

- **Język:** z użytkownikiem rozmawiaj po polsku. Dokumentacja (`docs/`, `README.md`), skille (`.claude/skills/`) i komentarze w kodzie są po angielsku.
- **C99:** bez `malloc`, bez blokowania, bez `stdio` w bibliotece. Każda funkcja ma czas wykonania ograniczony rozmiarem buforów.
- **Styl:** klamry zawsze, sufiksy `u`, jawne rzutowania. Kod kompiluje się czysto z `-Wall -Wextra -Wpedantic -Wconversion -Werror` i tak ma zostać.
- **API:** publiczne funkcje mają prefiks `Intercom_`, typy `Intercom*`. Wewnętrzne są w `ic_*` / `IC_*` i działają na instancji `ic_t`, żeby testy mogły uruchomić wiele węzłów w jednym procesie.
- **Nagłówki dla aplikacji:** tylko `include/Intercom.h` + `IntercomConfig.h` użytkownika. Nie dodawaj nowych publicznych nagłówków. Prywatne zostają w `src/` i są dołączane względnie.
- **Konfiguracja użytkownika:** `config/IntercomConfig_template.h` jest szablonem. Użytkownik kopiuje go do swojego projektu i nie edytuje w submodule.

## Struktura

```
include/Intercom.h          publiczne API + typy generowane z list wiadomości (X-macro)
config/                     szablon IntercomConfig.h
src/ic_internal.h           ic_t, format ramki, stałe, kontrole konfiguracji (#error)
src/ic_core.c               parser, nadajnik, link layer (FSM master/slave), pakowanie TLV, dedup, TTL
src/ic_queue.c              ring buffer rekordów o zmiennej długości (kolejki EVENT)
src/ic_crc.c                CRC-16/CCITT-FALSE
src/Intercom.c              publiczne API nad jedną statyczną instancją
tests/sim_test.c            symulator magistrali (wiele węzłów, czas bajtów 9600, szum, martwy węzeł)
tests/run_seeds.sh|.ps1     uruchomienie testów na wielu seedach
CMakeLists.txt              projekt główny: flagi, opcje, testy, cele analizy (cppcheck, MISRA, CodeQL)
intercom.cmake              definicja targetu `intercom` (też do include() w projekcie aplikacji)
cmake/                      version (wersja z taga git vX.Y.Z), sim_tests, misra_check
CMakePresets.json           INTERCOM[Debug|Release|SimTests|SimTestsSanitize] → build/<preset>/
.vscode/tasks.json          zadania VS Code: build, testy, analiza
docs/                       index, getting-started, protocol, architecture, configuration, api, porting, testing
```

## Komendy

```sh
cmake --preset "INTERCOM[SimTests]" && cmake --build "build/INTERCOM[SimTests]"
"./build/INTERCOM[SimTests]/intercom_sim_test"          # jeden przebieg (domyślny seed)
"./build/INTERCOM[SimTests]/intercom_sim_test" 12345    # konkretny seed
tests/run_seeds.sh "build/INTERCOM[SimTests]" 100       # 100 seedów (Windows: tests\run_seeds.ps1 -BuildDir ...)
cmake --build "build/INTERCOM[Debug]" --target cppcheck-analyze   # też misra-analyze, codeql-analyze
```

Szczegóły testowania są w skillu `intercom-testing`.

## Skille projektu (`.claude/skills/`)

| Skill | Kiedy |
| --- | --- |
| `intercom-testing` | Budowanie, uruchamianie i dopisywanie testów; weryfikacja po każdej zmianie |
| `intercom-core-change` | Każda zmiana w `src/` (protokół, kolejki, FSM, pakowanie). Zawiera niezmienniki, których nie wolno złamać |
| `intercom-port` | Integracja z konkretnym MCU / UART / RTOS w projekcie aplikacji |

## Stan (2026-10-14, v1.0.0)

- **Testy:** 8 scenariuszy symulacji przechodzi na 100 seedach z ASan/UBSan.
- **Rozmiar:** ok. 6,6 KB flash i 1668 B RAM (Cortex-M3, `-Os`, konfiguracja z szablonu). Pomiar przez `clang --target=thumbv7m`, bez `arm-none-eabi-gcc`.
- **Brak testów na sprzęcie.** Przykład portu STM32 w `docs/porting.md` nie był kompilowany.

## Decyzje projektowe (historia)

1. **POLL jako token.** Tylko odpytany slave nadaje przez maksymalnie `slotTimeMs`; inne węzły wysyłają wyłącznie ACK na DATA do siebie. Daje to determinizm bez arbitrażu.
2. **Dwie klasy wiadomości.**
   - EVENT: kolejki HIGH i NORMAL, dostarczanie dokładnie raz, FIFO na parę (nadawca, odbiorca, priorytet).
   - STATE: mailbox, najnowsza wartość wygrywa, nie może się przepełnić.
3. **Pakowanie TLV.** Wiele wiadomości w jednej ramce, jeden ACK na ramkę. Przy 9600 bit/s największy zysk daje ograniczenie narzutu.
4. **Ruch z masterem bez osobnych ACK.**
   - slave → master: w RESP, potwierdzane przez `ACKB` w następnym POLL,
   - master → slave: w POLL, potwierdzane przez `ACKB` w RESP tego samego slotu.
5. **Stop-and-wait na kanał.** 4-bitowy SEQ i flaga FIRST. Retransmisja jest odbudowywana z referencji do kolejki (peek/commit), bez bufora na ramkę.
6. **Błąd znaleziony testami (naprawiony).** Pusty RESP miał ten sam SEQ co następny RESP z treścią, więc master potwierdzał go z wyprzedzeniem i gubił dane. Teraz master potwierdza **tylko RESP z treścią**, a ramki bez treści nie biorą udziału w numeracji.
7. **Niedostępne węzły.**
   - Po `IC_DEAD_STREAK` (3) porażkach slave obsługuje węzeł na końcu slotu i bez retry.
   - Master odpytuje go co `IC_DEAD_POLL_DIV` (4) cykl.
   - Odbiorcy w slocie są obsługiwani round robin, co zapobiega zagłodzeniu.
8. **`onDrop(msgId, dst, missedNodes)`.** Maska oznacza węzły, które *nie potwierdziły* zdarzenia. Węzeł mógł je dostać, jeśli zginął ACK. Nie ma cichych strat.
9. **Odrzucona opcja: broadcast + ACK w szczelinach czasowych.** Byłaby wydajniejsza przy wielu odbiorcach, ale bardziej złożona. Do rozważenia jako v2.

## Otwarte tematy / możliwe następne kroki

- Port na docelowy MCU i test na sprzęcie (skill `intercom-port`).
- **MCU 16-bit (AVR, MSP430).** Kod nie zakłada 32-bitowego `int`, ale nie był na nich kompilowany. Sprawdź ostrzeżenia i atomowość odczytu `stDirty` (32-bit) bez sekcji krytycznej.
- **Restart węzła w trakcie pracy.** Logika flagi FIRST istnieje, ale scenariusz nie ma testu w symulatorze.
- Inicjalizacja repozytorium git (repo jeszcze nie ma).
- Optymalizacja v2: broadcast DATA z ACK w szczelinach; opcjonalne okresowe odświeżanie stanów.
