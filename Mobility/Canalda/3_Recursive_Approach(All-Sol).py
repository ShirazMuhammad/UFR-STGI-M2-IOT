def solve_recursive_all(coins, target):
    """
    Exhaustively searches the state space using indexed backtracking
    to collect all distinct coin combinations summing to the target.
    """
    all_solutions = []

    def backtrack(remaining, path, start_index):
        remaining = round(remaining, 2)
        if remaining == 0:
            all_solutions.append(list(path))
            return
        if remaining < 0:
            return

        # start_index prevents duplicate permutations
        for i in range(start_index, len(coins)):
            coin = coins[i]
            path.append(coin)
            backtrack(remaining - coin, path, i)
            path.pop()  # Backtrack

    print("=" * 65)
    print("EXERCISE 3: RECURSIVE APPROACH (All Unique Solutions)")
    print("=" * 65)

    backtrack(target, [], 0)

    status = f"SUCCESS - {len(all_solutions)} Unique Solutions Found" if all_solutions else "FAILED"

    print(f"Status:            {status}")
    print(f"Coins Available:   {coins}")
    print(f"Target Amount:     {target}")
    print("\nGenerated Combinations:")
    for idx, sol in enumerate(all_solutions, 1):
        # FIXED: Wrapped 'sol' in str() so width formatting '<30' works correctly
        print(f"  Combination {idx}: {str(sol):<30} | Sum: {round(sum(sol), 2)}")
    print("=" * 65)
    return all_solutions


# Run Exercise 3
coins_available = [5, 2, 1.5]
target_amount = 9.5
solve_recursive_all(coins_available, target_amount)