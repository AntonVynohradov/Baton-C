# Baton-C: kontekst projektu

Biblioteka C11 (static lib) do deterministycznej komunikacji master/slave po jednoprzewodowym UART (half-duplex, domyślnie 9600 bit/s) dla systemów embedded. Master przekazuje „pałeczkę” (POLL) kolejnym slave'om. Podłączana do projektów jako git submodule. Następca wewnętrznej biblioteki `intercom` (format ramki i reguły dostarczania bez zmian).

Projekt jest zbudowany według wzorca firmy SystemFromScratch® (skill `sfs-protocol-library`, implementacja referencyjna DXL2-C): warstwy defs → codec → link → queue → node, MISRA C:2012, 100% pokrycia, Python twin, przykłady, MkDocs + PDF, CI.

## Konwencje

- **Język:** z użytkownikiem rozmawiaj po polsku. Dokumentacja (`docs/`, `README.md`), skille (`.claude/skills/`) i komentarze w kodzie są po angielsku.
- **C11:** bez `malloc`, bez blokowania, bez globalnego stanu, bez odczytu zegara (czas przychodzi jako `now_ms`), bez `stdio`. Tylko `<stddef.h>`, `<stdint.h>`, `<stdbool.h>`, `<string.h>`. Każda funkcja ma czas wykonania ograniczony rozmiarem buforów.
- **MISRA C:2012:** jeden `return` na funkcję, nawiasy w wyrażeniach, `(void)` przy ignorowanych wynikach, brak arytmetyki wskaźników i rzutowań między typami obiektów, brak `break` w pętlach (flagi). `misra-analyze` = 0 naruszeń; odstępstwa w `docs/misra.md`.
- **Styl:** `.clang-format` (LLVM, 4 spacje, Allman, 100 kolumn, `uint8_t* p`), sufiksy `U`, jawne rzutowania, bannery plików jak w istniejących. Czysta kompilacja z `-Wall -Wextra -Wpedantic -Wconversion` (testy z `-Werror`).
- **API:** symbole `baton_` / `BATON_`, struktury `struct baton_…` (bez typedef), callbacki typedef. Aplikacja używa 8 funkcji `baton_node_*` na własnej `struct baton_node`. Kody błędów `BATON_OK`/`BATON_ERR_*` (int).
- **Nagłówki:** `baton.h` (parasol) + `inc/*.h`. Konfiguracja aplikacji: `baton_user_config.h` (listy wiadomości X-macro + rozmiary), kopia `config/baton_user_config_template.h`; szablonu nie edytuje się w submodule.
- **Brak martwego kodu:** gałąź nieosiągalna przez API = usunąć i opisać niezmiennik w komentarzu (bramka 100% gałęzi).
- **Liczby w dokumentacji tylko zmierzone** (stos, rozmiar, RAM, pokrycie); po zmianach zmierzyć ponownie.

## Struktura

```
baton.h                     parasol, wersja
inc/                        baton_config.h, baton_defs.h, baton_protocol.h, baton_link.h, baton_queue.h, baton_node.h
src/                        baton_protocol.c (CRC, parser, enkoder), baton_link.c (ring RX, timeout, nadajnik/echo),
                            baton_queue.c (rekordy zdarzeń), baton_node.c (FSM master/slave, pakowanie TLV, dedup, TTL)
config/                     szablon baton_user_config.h
tests/unit/                 GoogleTest (gtest_baton_*.cpp, baton_test_support.*), rejestracja w baton.cmake
tests/sim/                  symulator magistrali (testy BusSim + runner baton_sim)
tests/integration/          pytest + Python twin (baton/protocol.py), cross-check C/Python
tests/config/               konfiguracja testów i analizy
tests/run_seeds.sh|.ps1     symulacja na wielu seedach
examples/                   bare_metal (test w CI na symulowanej magistrali), host_serial (master na porcie PC), zephyr (niekompilowany)
tools/docs/                 diagrams.py, build_manual.py (PDF), stack_usage.py, check_snippets.py, svgkit.py
docs/ + mkdocs.yml + overrides/   strona MkDocs Material w stylu SystemFromScratch®, źródło PDF
cmake/                      version (tag vX.Y.Z), unit_tests, integration_tests, misra_check
baton.cmake                 target `baton` (include() w projekcie aplikacji, BATON_CONFIG_DIR)
CMakePresets.json           BATON[Debug|Release|UnitTests|IntegrationTests|Examples|Sanitize] → build/<preset>/
.github/workflows/          build, unit tests + coverage + seedy z ASan, integracja, analiza, format, docs, SBOM, release
```

## Komendy

```sh
cmake --preset "BATON[Debug]" && cmake --build "build/BATON[Debug]"
cmake --preset "BATON[UnitTests]" && cmake --build "build/BATON[UnitTests]" --target coverage   # bramka 100%
cmake --build "build/BATON[UnitTests]" --target baton_sim && tests/run_seeds.sh "build/BATON[UnitTests]" 100
cmake --preset "BATON[IntegrationTests]" -DPython3_EXECUTABLE=<python z pytest> && cmake --build "build/BATON[IntegrationTests]" --target integration_tests
cmake --build "build/BATON[Debug]" --target misra-analyze          # też cppcheck-analyze, codeql-analyze
cmake --preset "BATON[Examples]" && cmake --build "build/BATON[Examples]" && ctest --test-dir "build/BATON[Examples]" -L example
python tools/docs/check_snippets.py && py -3.14 -m mkdocs build && py -3.14 tools/docs/build_manual.py
```

Lokalnie (Windows): pytest, mkdocs i WeasyPrint są w `py -3.14`; MinGW nie ma ASan — sanitizery i pełny pipeline CI sprawdza się w kontenerze `ubuntu:24.04` (w Git Bash `MSYS_NO_PATHCONV=1` dla `docker run -v`).

## Skille projektu (`.claude/skills/`)

| Skill | Kiedy |
| --- | --- |
| `baton-core-change` | Każda zmiana w `src/`, `inc/`, `baton.h`. Niezmienniki N1–N22, reguły, definicja „done” |
| `baton-testing` | Budowanie i uruchamianie testów, pokrycie, seedy, Python twin, pomiary |
| `baton-port` | Integracja z konkretnym MCU / UART / RTOS w projekcie aplikacji |
| `sfs-protocol-library` (globalny) | Wzorzec firmy: dokumentacja, strona, PDF, CI, release |

## Stan (2026-10-25, przygotowane do 1.0.0, bez tagu)

- **Testy:** 132 testy GoogleTest (w tym 8 scenariuszy symulacji), 100% linii (1053) i gałęzi (601) `src/`; symulacja 100/100 seedów; 11 testów pytest (cross-check 300 ramek); przykład bare-metal przechodzi.
- **Analiza:** MISRA 0 naruszeń (Cppcheck 2.16.2), 4 odstępstwa (DEV-01..04, X-macro tłumione komentarzami); Cppcheck 0 uwag.
- **Rozmiar:** 6620 B kodu (Cortex-M4 `-Os`, GCC 15.2), 1696 B RAM na węzeł (szablon), stos `baton_node_process()` 280 B (`-Os`).
- **Brak testów na sprzęcie.** Przykład Zephyr i porty STM32 w skillu nie były kompilowane.
- Repozytorium git ma tylko commit początkowy (`intercom`); zmiany Baton-C nie są jeszcze zacommitowane.

## Decyzje projektowe (historia)

1. **POLL jako token (pałeczka).** Tylko odpytany slave nadaje przez maksymalnie `slot_time_ms`; inne węzły wysyłają wyłącznie ACK na DATA do siebie. Determinizm bez arbitrażu.
2. **Dwie klasy wiadomości.** EVENT: kolejki HIGH i NORMAL, dokładnie raz, FIFO na parę (nadawca, odbiorca, priorytet). STATE: mailbox, najnowsza wartość wygrywa, nie może się przepełnić.
3. **Pakowanie TLV.** Wiele wiadomości w jednej ramce, jeden ACK na ramkę.
4. **Ruch z masterem bez osobnych ACK.** slave → master w RESP, potwierdzane `ACKB` w następnym POLL; master → slave w POLL, potwierdzane `ACKB` w RESP tego samego slotu.
5. **Stop-and-wait na kanał.** 4-bitowy SEQ i flaga FIRST. Retransmisja odbudowywana z referencji do kolejki, bez bufora na ramkę.
6. **Błąd znaleziony testami (naprawiony w intercom).** Pusty RESP miał ten sam SEQ co następny RESP z treścią; master potwierdza **tylko RESP z treścią**, ramki bez treści nie biorą udziału w numeracji.
7. **Niedostępne węzły.** Po `BATON_DEAD_STREAK` (3) porażkach slave obsługuje węzeł na końcu slotu i bez retry; master odpytuje go co `BATON_DEAD_POLL_DIV` (4) cykl; round robin w slocie.
8. **`on_drop(msg_id, dst, missed_nodes)`.** Maska węzłów, które *nie potwierdziły*. Nie ma cichych strat.
9. **Odrzucona opcja: broadcast + ACK w szczelinach.** Do rozważenia w v2.
10. **Baton-C (2026-10-25):** przejście na wzorzec SystemFromScratch®: C11, API instancyjne (bez statycznej instancji), jawny `now_ms` zamiast `getTimeMs`, `send` stempluje TTL czasem ostatniego `process`, warstwy link/node rozdzielone (link zwraca zdarzenia `BATON_LINK_FRAME`/`TX_DONE`), rekordy kolejki serializowane bajtowo (MISRA 11.3), unikalność ID i pojemność kolejek sprawdzane w czasie kompilacji (`switch` + `_Static_assert`), martwe gałęzie usunięte z udokumentowanymi niezmiennikami.

## Otwarte tematy / możliwe następne kroki

- Port na docelowy MCU i test na sprzęcie (skill `baton-port`).
- Kompilacja przykładu Zephyr w workspace west.
- MCU 8/16-bit (AVR, MSP430): ostrzeżenia, atomowość 32-bitowych masek.
- Scenariusz symulacji: restart węzła w trakcie pracy (flaga FIRST).
- Pierwszy commit Baton-C, publikacja repo `AntonVynohradov/Baton-C`, GitHub Pages, tag `v1.0.0` (CHANGELOG `[Unreleased]` → `[1.0.0]`).
- v2: broadcast DATA z ACK w szczelinach; opcjonalne okresowe odświeżanie stanów.
