# Contributing to BlueLink Manager

We welcome contributions to the BlueLink Manager project! By contributing, you agree to abide by our [Code of Conduct](CODE_OF_CONDUCT.md) and the terms of the [MIT License](LICENSE).

## How to Contribute

### 1. Fork the Repository

Fork the [BlueLink Manager repository on GitHub](https://github.com/kawacukennedy/beam).

### 2. Clone Your Fork

```bash
git clone https://github.com/YOUR_USERNAME/beam.git
cd beam/BlueBeamNative
```

### 3. Create a New Branch

Create a new branch for your feature or bug fix. Use a descriptive name.

```bash
git checkout -b feature/your-feature-name
# or
git checkout -b bugfix/fix-bug-description
```

### 4. Make Your Changes

-   Ensure your code adheres to the existing coding style and conventions.
-   Add comments where necessary to explain complex logic.
-   Write clear, concise commit messages.
-   If you're adding a new feature, consider writing unit tests.

### 5. Test Your Changes

Before submitting a pull request, ensure your changes build successfully and all existing tests pass. If you added new tests, make sure they also pass.

To build the project:

```bash
mkdir build
cd build
cmake ..
cmake --build .
```

### 6. Commit Your Changes

```bash
git add .
git commit -m "feat: Add new feature" # or "fix: Fix bug"
```

### 7. Push to Your Fork

```bash
git push origin feature/your-feature-name
```

### 8. Create a Pull Request

Go to the original [BlueLink Manager repository on GitHub](https://github.com/kawacennedy/beam) and open a new pull request from your forked branch to the `main` branch.

-   Provide a clear and detailed description of your changes.
-   Reference any related issues.

## Code of Conduct

We expect all contributors to adhere to our [Code of Conduct](CODE_OF_CONDUCT.md).

## Reporting Bugs

If you find a bug, please open an issue on the [GitHub Issues page](https://github.com/kawacennedy/beam/issues) and provide as much detail as possible, including steps to reproduce the bug.

## Feature Requests

If you have an idea for a new feature, please open an issue on the [GitHub Issues page](https://github.com/kawacennedy/beam/issues) to discuss it.
