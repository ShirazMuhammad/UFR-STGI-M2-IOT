def solve_recursive_single(coins, target):
    """
    Depth-First Search (Backtracking) that returns the first valid
    combination that exactly equals the target amount.
    """

    def backtrack(remaining, path):
        remaining = round(remaining, 2)
        if remaining == 0:
            return path  # Single solution reached
        if remaining < 0:
            return None  # Exceeded target, trigger backtracking

        for coin in coins:
            result = backtrack(remaining - coin, path + [coin])
            if result is not None:
                return result  # Immediately return the first found solution
        return None

    print("=" * 65)
    print("EXERCISE 2: RECURSIVE APPROACH (Single Solution)")
    print("=" * 65)

    solution = backtrack(target, [])
    status = "SUCCESS - Single Solution Found" if solution else "FAILED - No Solution"

    print(f"Status:            {status}")
    print(f"Coins Available:   {coins}")
    print(f"Target Amount:     {target}")
    print(f"Solution Path:     {solution}")
    print(f"Sum Verification:  {round(sum(solution), 2) if solution else 0}")
    print("=" * 65)
    return solution


# Run Exercise 2
coins_available = [5, 2, 1.5]
target_amount = 14.5
solve_recursive_single(coins_available, target_amount)