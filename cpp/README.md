# Level 6 — optymalizator C++20

Uruchamianie z katalogu głównego repozytorium. Kod bazuje na solverze kolegi
z commita `e6d1c9c`; pliki z tego commita są zachowane w `baseline_e6d1c9c/`.
Najlepsze kompletne rozwiązania znajdują się w `outputs/`.

Aktualny zakres optymalizacji obejmuje wyłącznie `large` i `enormous`.
Wyniki wybieramy osobno dla każdego przypadku; poszczególne pliki nie muszą
pochodzić z jednego solvera. Dla `small` i `medium` nie uruchamiamy nowych
poszukiwań; gotowe lepsze wyniki pobrane z master są zachowywane.

## Osobne solvery dla dużych zestawów

```sh
bash cpp/build_specialized.sh
./cpp/level6_trajectories names=4-enormous freed=3 explore=1 \
  width=300 trials=240 drop=4 seed=467 threads=3 \
  outdir=cpp/candidates/explore_enormous467
./cpp/level6_trajectories names=3-large mode=segments freed=3 \
  width=400 trials=240 seed=457 threads=3 \
  outdir=cpp/candidates/segments_large457
./cpp/level6_trajectories names=3-large,4-enormous mode=windows freed=4 \
  window=24 explore=1 width=500 trials=200 seed=523 threads=3 \
  outdir=cpp/candidates/windows523
./cpp/level6_retime names=3-large,4-enormous width=800 trials=48 \
  seed=439 threads=3 outdir=cpp/candidates/retime439
```

`level6_trajectories` przebudowuje całe trasy dwóch, trzech lub czterech emiterów
(`freed=2`, `3` albo `4`),
pozostawiając pozostałe ścieżki jako ograniczenia. Sprawdzenie możliwości
przyszłego utworzenia i usunięcia materii odrzuca gałęzie, które nie mogą
już zakończyć zadania. `mode=segments` przebudowuje odcinki między odbiorami,
zachowując pozycje końcowe potrzebne do podłączenia dalszej części programu.
Ostatni odcinek może zakończyć się w dowolnej konfiguracji.

`mode=windows` wybiera krótkie fragmenty także wewnątrz budowania wzoru
i przez granice odbiorów. Odtwarza stan materii na początku fragmentu,
a na końcu wymaga zgodności materii oraz pozycji i aktywności emiterów.
Pozwala to bezpiecznie podłączyć nieruszoną końcówkę. `window=24` ogranicza
długość przebudowywanego fragmentu. Każdy wynik przechodzi symulację całego
programu, również w oryginalnej reprezentacji planszy.

`symmetry=1` w trybach `windows` i `segments` dopuszcza zamianę ról
przeciwległych emiterów na końcu przebudowy. Pozycje są odzwierciedlane,
a instrukcje dalszej części zamieniane wraz z odwróceniem ruchów `F`/`B`.
Wiązki pozostają równoważne, ale emitery mogą dotrzeć do takiego zakończenia
krótszą drogą. Zamiana obejmuje tylko emitery przebudowywane w danej próbie.

`explore=1` dopuszcza poprawne rozwiązania tej samej długości i krótkie
wydłużenia jako punkty startowe następnych prób. Zapisuje wyłącznie najlepszy
znaleziony wynik; przejściowe pogorszenia nie trafiają do plików wynikowych.
`minlen=30` kieruje budżet na programy mające przynajmniej 30 kroków.
`portfolio=1 explore=1` w trybie `whole` dodaje restarty z różnych starszych
poprawnych rozwiązań znalezionych w katalogach kandydatów i zachowanych
wersjach bazowych. Dłuższe trasy mogą być skracane stopniowo; nie zastępują
najlepszego wyniku, dopóki rzeczywiście go nie poprawią.

Wybór kolejnej wiązki wyszukiwania używa częściowego wyboru zamiast sortowania
wszystkich następców. W razie dużej liczby duplikatów rozszerza rozpatrywany
prefiks, zachowując ten sam wybór najlepszych unikalnych stanów.

`level6_retime` dobiera od nowa czas wykonania sześciu strumieni instrukcji,
zachowując ich kolejność w obrębie emitera. Próbuje też usuwać pary ruchów
o zerowym łącznym przesunięciu. Oba solvery używają gęstych masek wyłącznie
istotnych pól i sprawdzają wynik również na oryginalnej reprezentacji planszy.

## Budowanie i sprawdzanie

Linux / WSL (sprawdzone z GCC 13.3):

```sh
bash cpp/build.sh
./cpp/level6_opt verify baseline=cpp/outputs
node python/verify_level6.cjs cpp/outputs
```

Z PowerShell w tym środowisku:

```powershell
wsl -d Ubuntu --cd /mnt/c/Home/CCC2 -- bash cpp/build.sh
wsl -d Ubuntu --cd /mnt/c/Home/CCC2 --exec ./cpp/level6_opt verify baseline=cpp/outputs
```

`build.ps1` jest alternatywą dla Visual C++ x64. Kompilacja MSVC działała,
ale uruchomienie pliku EXE zostało odrzucone przez lokalne środowisko;
pomiary i generowanie wyników wykonano przez WSL.

## Wyszukiwanie

```sh
./cpp/level6_opt beam baseline=cpp/outputs names=3-large,4-enormous \
  width=300 settings=8:0.5:4,20:0.8:8,5:0.15:3,2:1:2 \
  threads=4 slack=1 seed=31 tag=_run

./cpp/level6_opt merge baseline=cpp/outputs \
  sources=cpp/candidates/beam_run outdir=cpp/outputs
```

Kandydaci są zapisywani po każdej poprawie. `NONE` oznacza brak poprawy
danego przypadku, więc plików z `candidates/` nie należy zgłaszać bez scalenia.
`merge` symuluje każdego kandydata i zachowuje krótszy poprawny program.
Lista `sources` może zawierać wiele katalogów rozdzielonych przecinkami.
`cases=0-19` ogranicza eksperyment do pierwszych 20 przypadków.
Można też wybrać pojedynczy przypadek (`cases=167`) lub listę (`cases=35,50,167`).

Pozostałe tryby:

- `lns`: przebudowa końcówek istniejących programów; `width`, `settings`, `step`.
- `graph`: budowa grafu konfiguracji po odbiorach i wybór całej sekwencji przez programowanie dynamiczne; `maxnodes` ogranicza liczbę rozwijanych konfiguracji, a `segmentlimit` długość odcinka (0 dobiera ją z obecnego wyniku).
- `cycles`: szukanie okresowych sekwencji przez wyżarzanie; `ks`, `iterations`, `restarts`.
- `cycle2`: wyczerpujące szukanie cykli długości 2; koszt ogranicza `maxcost`.
- `polish`: usuwanie kroków i przesuwanie instrukcji przy zachowaniu poprawności.
- `repair`: skrócenie programu i naprawianie go z dopuszczeniem niepoprawnych stanów pośrednich.
- `recycle`: wyciąganie cykli z gotowego programu i skracanie drogi do ich rozpoczęcia; długość cyklu ogranicza program wejściowy, bez dawnego limitu 24 kroków.
- `reverse`: skracanie odcinków między odbiorami przez wyszukiwanie od końca.
- `reversefree`: skracanie ostatniego odcinka z możliwością zmiany końcowych pozycji emiterów.
- `cyclesa`: ocena cykli przez naruszenia wzoru, bez ograniczenia do prefiksów krótszych niż 8 kroków.
- `cyclesym`: cykle dopuszczające zamianę przeciwległych emiterów; `segs=3+3,3+4` określa długość okresu i liczbę odbiorów, a ich rozmieszczenie wybiera programowanie dynamiczne.

Eksperymentalne parametry beamu: `optical=2` ogranicza liczbę wariantów
różniących się wyłącznie pozycjami wyłączonych emiterów, `matter=16` limituje
liczbę stanów z identyczną materią, `capture=0` równoważy odbiory i częściowo
utworzony wzór, a `adaptive=1` zmienia karę za materię w sąsiedztwie wraz
z postępem budowania wzoru. Domyślnie te opcje są wyłączone. Wyniki tych
wariantów są oceniane osobno; nie każdy eksperyment poprawia rozwiązania.

`cross=1` włącza ocenę potencjalnych przecięć linii emiterów, przeniesioną
z nowszej wersji kolegi. Opcjonalna czwarta liczba w `settings`, np.
`3:0.3:2:1`, ustala tę wagę osobno dla danej konfiguracji parametrów.

`./cpp/level6_splice` zbiera odcinki z istniejących wyników i kandydatów,
utożsamia stany z zamienionymi przeciwległymi emiterami i szuka krótszych
połączeń przez programowanie dynamiczne. Jest to wersja C++ pomysłu
`python/level6_splice.py`; domyślnie zapisuje poprawy w
`cpp/candidates/splice_cpp`. Tak jak inne kandydaty wymagają one scalenia.
W pierwszej próbie na aktualnym komplecie samo łączenie nie dało poprawy.

### Dokładne wyszukiwanie z Z3

Opcjonalne solvery są również napisane w C++. Zależność Z3 jest rozpakowywana
lokalnie w `cpp/vendor/`; główny `level6_opt` jej nie wymaga.

```sh
bash cpp/setup_z3.sh       # Ubuntu / WSL, bez instalowania pakietów systemowych
bash cpp/build_sat.sh
./cpp/level6_sat mode=segments baseline=cpp/outputs windows=6,12,24 \
  timeout=2500 threads=2 outdir=cpp/candidates/sat_segments
./cpp/level6_sat_cycle names=2-medium specs=4:1,5:1,6:1 \
  timeout=15000 threads=2 outdir=cpp/candidates/sat_cycles
```

`level6_sat` skraca ostatni odbiór (`mode=suffix`) albo odcinki między
odbiorami (`mode=segments`). Zachowuje nieruszony prefiks, modeluje operacje
emiterów i tworzenie/usuwanie materii, a gotowy program ponownie symuluje.
`level6_sat_cycle` szuka zamkniętych cykli, także z zamianą przeciwległych
emiterów; `specs=5:2` oznacza 5 kroków i co najmniej 2 odbiory.
`timeout` jest limitem jednej próby w milisekundach. Brak wyniku w limicie
czasu nie jest dowodem optymalności. Model ogranicza pozycje do dwóch
boków graniczących z pozycją początkową emitera.

## Co zmieniono

Solver kolegi łączy przeszukiwanie wiązkowe, wyszukiwanie cykli i lokalne
skracanie programów. Dwie przeciwległe wiązki są grupowane w oś, co pozwala
oceniać sześć emiterów przez operacje na maskach bitowych.

Ta wersja dobiera liczbę 64-bitowych słów do rozmiaru planszy. Usuwa też
materię spoza wzoru i jego sąsiedztwa ze stanu przeszukiwania: nie wpływa ona
na odbiór wzoru ani przebieg wiązek. Przy usuwaniu duplikatów uwzględnia
zamienność przeciwległych emiterów. Opcjonalne `history=1` pomija również
stany zachowane na wcześniejszych głębokościach.

Dodano naprawianie skróconych harmonogramów z pamięcią stanów prefiksu,
wydobywanie cykli z gotowych rozwiązań, scalanie i weryfikację w C++,
przenośne budowanie oraz zapisywanie postępu.

Na próbie medium 0–9, przy width=100, settings=3:0.3:2, seed=1 i jednym
wątku, specjalizacja masek skróciła czas z 1,606 s do 0,726 s. To pomiar
tej konkretnej próby; nie oznacza takiego samego przyspieszenia każdego trybu.

## Wyniki

`results.json` zawiera porównanie wszystkich zestawów i liczbę poprawionych
przypadków. Punktacja medium + large + enormous jest podana oddzielnie od
sumy wszystkich pięciu plików. Punktem odniesienia jest commit `e6d1c9c`, z wynikiem
15 587 i 15 715 instrukcji. Wszystkie pliki końcowe są sprawdzane logiką
oryginalnego symulatora z `level-6/sandbox-level-6.html`.

Najlepszy znaleziony komplet, sprawdzony na wszystkich 553 programach:

| Zestaw | e6d1c9c | Ta wersja |
| --- | ---: | ---: |
| example | 20 | 20 |
| small | 108 | 108 |
| medium | 5907 | 5831 |
| large | 5069 | 4812 |
| enormous | 4611 | 4328 |
| medium + large + enormous | 15587 | **14971** |
| Wszystkie pliki | 15715 | **15099** |

Poprawiono 216 przypadków; żaden nie ma dłuższego programu niż w bazie `e6d1c9c`.
Identyczne pliki zapisano również w `python/outputs/`, a komplet pięciu
plików do zgłoszenia jest w `cpp/level6_submission.zip`.

`cpp/package_results.ps1` sprawdza wyniki oryginalnym symulatorem, odświeża
obie kopie wyników, tabelę, raport JSON i ZIP oraz porównuje zawartość ZIP
z plikami źródłowymi. Parametr `-Node` pozwala wskazać ścieżkę do Node.js.

Są to wyniki wyszukiwania heurystycznego; nie stanowią dowodu optymalności.
