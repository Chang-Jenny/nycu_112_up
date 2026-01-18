import random

def generate_maze(rows, cols, probability):
    maze = [['#'] * cols for _ in range(rows)]

    # 隨機選擇起點和終點的位置，確保它們不在邊緣上
    start_row, start_col = random.randint(1, rows - 2), random.randint(1, cols - 2)
    end_row, end_col = random.randint(1, rows - 2), random.randint(1, cols - 2)

    maze[start_row][start_col] = 'S'
    dfs(maze, start_row, start_col, end_row, end_col, probability)

    maze[end_row][end_col] = 'E'
    return maze

def dfs(maze, row, col, end_row, end_col, probability):
    directions = [(0, 1), (1, 0), (0, -1), (-1, 0)]
    random.shuffle(directions) # random order

    for dr, dc in directions:
        new_row, new_col = row + 2 * dr, col + 2 * dc

        if 0 < new_row < len(maze) - 1 and 0 < new_col < len(maze[0]) - 1:
            if maze[new_row][new_col] == '#':
                maze[row + dr][col + dc] = ' '
                maze[new_row][new_col] = ' '

                dfs(maze, new_row, new_col, end_row, end_col, probability)

# 設定迷宮大小和機率
rows, cols = 10, 20
probability_of_path = 0.7

maze = generate_maze(rows, cols, probability_of_path)

# 輸出迷宮
for row in maze:
    print(' '.join(row))