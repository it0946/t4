import sys

if len(sys.argv) != 2:
    print("Usage: fadw [filename]")
    sys.exit(1)

with open("sorted.bin", "r") as f:
    eng = {w for w in f.read().split('\0')}

try:
    non_english = 0
    num_total = 0
    num_unique = 0

    with open(sys.argv[1], "r") as f:
        buf = f.read()

    inp = set()

    s = ""
    for c in buf:
        if not c.isalpha():
            if len(s) != 0:
                num_total += 1
                if not s in inp:
                    num_unique += 1                    
                    inp.add(s)

                    if not s in eng:
                        non_english += 1
                        print(s)
            s = ""
        else:
            s += c.lower()

    print(f"\nTotal words: {num_total}")
    print(f"Unique words: {num_unique}")
    print(f"Number of non-english words: {non_english}")

except Exception as e:
    print(f"Error: {e}")
