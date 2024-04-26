arr = []
# read from file 'results.txt'
with open("results_2.txt", "r") as file:
    arr = file.readlines()


arr_actual_cads = [150, 98, 58, 32,18,138,93, 56,31,17,165,118,74,42,23,51,45,37,27,17]
amount_of_packets = 10
arr_test = []
i = 0
while i < len(arr):
    arr_test.append(arr[i][:-1])

    i += 1

    success = 0
    failed = 0
    actual_cads = arr_actual_cads[int(i /(amount_of_packets + 1))]
    while i < len(arr) and not arr[i].startswith("Testing"):
        success += arr[i].count("1")
        failed += actual_cads - success
        i += 1

    success = int(success /  amount_of_packets) 
    print(f"{arr_test[int(i / (amount_of_packets + 1) - 1)]:<70}: {success}/{actual_cads} ({success/actual_cads*100:.2f}%)")
