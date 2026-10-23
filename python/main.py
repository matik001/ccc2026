import sys
from io import StringIO
from pathlib import Path
from solution import solution


def main():
    # Ścieżki do katalogów
    inputs_dir = Path('inputs')
    outputs_dir = Path('outputs')

    # Upewnij się, że katalog outputs istnieje
    outputs_dir.mkdir(exist_ok=True)

    # Przetwórz każdy plik w katalogu inputs
    for input_file in inputs_dir.iterdir():
        if input_file.is_file():
            # Wczytaj dane z pliku wejściowego
            with open(input_file, 'r') as f:
                data = f.read().strip().split('\n')

                # Spróbuj skonwertować każdą linię na int
                converted_data = []
                for line in data:
                    try:
                        # Jeśli da się przekonwertować na int, zrób to
                        converted_data.append(int(line))
                    except ValueError:
                        # Jeśli nie, zostaw jako string
                        converted_data.append(line)

                data = converted_data

            # Przechwytuj output z print()
            old_stdout = sys.stdout
            sys.stdout = captured_output = StringIO()

            try:
                # Uruchom funkcję solution
                solution(data)
            finally:
                # Przywróć normalny stdout
                sys.stdout = old_stdout

            # Pobierz przechwycony output
            output_text = captured_output.getvalue()

            # Utwórz nazwę pliku wyjściowego (taka sama jak wejściowy)
            output_file = outputs_dir / input_file.name

            # Zapisz wynik do pliku wyjściowego
            with open(output_file, 'w') as f:
                f.write(output_text)

            print(f"Przetworzono: {input_file.name} -> {output_file.name}")


if __name__ == '__main__':
    main()
