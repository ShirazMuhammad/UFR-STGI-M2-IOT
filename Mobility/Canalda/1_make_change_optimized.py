def solve_iterative_greedy(coins, target):
    """
    Solves the target sum iteratively using a greedy approach.
    Sorts coins descending and picks the largest possible fit at each step.
    """
    coins_sorted = sorted(coins, reverse=True)
    current_sum = 0
    solution = []

    print("-" * 60)
    print(f"EXERCISE 1: ITERATIVE GREEDY ALGORITHM (Target: {target})")
    print("-" * 60)

    for coin in coins_sorted:
        while current_sum + coin <= target:
            current_sum += coin
            solution.append(coin)

    status = "SUCCESS" if current_sum == target else "PARTIAL / NO EXACT MATCH"

    print(f"Status: {status}")
    print(f"Coins Used: {solution}")
    print(f"Total Accumulated: {current_sum}")
    print("-" * 60)
    return solution


# Execution test
coins_available = [5, 2, 1.5]
target_amount = 9.5
solve_iterative_greedy(coins_available, target_amount)