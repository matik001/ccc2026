# Level 6 — optymalizator C++20

Uruchamianie z katalogu głównego repozytorium. Kod bazuje na solverze kolegi
z commita `e6d1c9c`; pliki z tego commita są zachowane w `baseline_e6d1c9c/`.
Najlepsze kompletne rozwiązania znajdują się w `outputs/`.

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
./cpp/level6_opt beam baseline=cpp/outputs names=2-medium,3-large,4-enormous \
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

Pozostałe tryby:

- `lns`: przebudowa końcówek istniejących programów; `width`, `settings`, `step`.
- `cycles`: szukanie okresowych sekwencji przez wyżarzanie; `ks`, `iterations`, `restarts`.
- `cycle2`: wyczerpujące szukanie cykli długości 2; koszt ogranicza `maxcost`.
- `polish`: usuwanie kroków i przesuwanie instrukcji przy zachowaniu poprawności.
- `repair`: skrócenie programu i naprawianie go z dopuszczeniem niepoprawnych stanów pośrednich.
- `recycle`: wyciąganie cykli z gotowego programu i skracanie drogi do ich rozpoczęcia.
- `reverse`: skracanie odcinków między odbiorami przez wyszukiwanie od końca.
- `cyclesa`: ocena cykli przez naruszenia wzoru, bez ograniczenia do prefiksów krótszych niż 8 kroków.
- `cyclesym`: cykle dopuszczające zamianę przeciwległych emiterów; `segs=3+3,3+4` określa długość okresu i liczbę odbiorów, a ich rozmieszczenie wybiera programowanie dynamiczne.

Eksperymentalne parametry beamu: `optical=2` ogranicza liczbę wariantów
różniących się wyłącznie pozycjami wyłączonych emiterów, `matter=16` limituje
liczbę stanów z identyczną materią, `capture=0` równoważy odbiory i częściowo
utworzony wzór, a `adaptive=1` zmienia karę za materię w sąsiedztwie wraz
z postępem budowania wzoru. Domyślnie te opcje są wyłączone. Wyniki tych
wariantów są oceniane osobno; nie każdy eksperyment poprawia rozwiązania.

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
sumy wszystkich pięciu plików. Oryginalna wersja z master ma odpowiednio
15 587 i 15 715 instrukcji. Wszystkie pliki końcowe są sprawdzane logiką
oryginalnego symulatora z `level-6/sandbox-level-6.html`.

Stan zapisany do wysłania po weryfikacji 553 programów; optymalizacja trwa:

| Zestaw | Master | Ta wersja |
| --- | ---: | ---: |
| example | 20 | 20 |
| small | 108 | 108 |
| medium | 5907 | 5852 |
| large | 5069 | 4971 |
| enormous | 4611 | 4533 |
| medium + large + enormous | 15587 | **15356** |
| Wszystkie pliki | 15715 | **15484** |

Poprawiono 107 przypadków; żaden nie ma dłuższego programu niż na pobranym master.
Identyczne pliki zapisano również w `python/outputs/`, a komplet pięciu
plików do zgłoszenia jest w `cpp/level6_submission.zip`.

Są to wyniki wyszukiwania heurystycznego; nie stanowią dowodu optymalności.
