# Contributing

Contributions to Baldr are welcome — bug reports, documentation fixes, and pull requests all help.

## Reporting bugs

Open an issue on the [GitHub issue tracker](https://github.com/gilmar-sales/Baldr/issues) and include:

- A clear, descriptive title.
- A minimal reproduction (a small program that triggers the bug, or steps to reproduce against an existing example).
- The compiler, OS, and Baldr version you are using.

## Improving the documentation

If you spot a typo, broken example, or unclear explanation in this site, please open an issue or a pull request against the [`docs/`](https://github.com/gilmar-sales/Baldr/tree/main/docs) directory. The documentation is built with [Zensical](https://zensical.org) and uses the same Markdown conventions as the rest of the project.

## Pull requests

1. Fork the repository and create a branch off `main`.
2. Make your changes — keep commits focused and write clear messages.
3. Make sure the project still builds:
    ```bash
    cmake -S . -B build
    cmake --build build
    ```
4. Open a pull request against `main` and describe what you changed and why.

## Code of conduct

By participating, you agree to abide by the [GitHub Community Guidelines](https://docs.github.com/en/site-policy/github-terms/github-community-guidelines). Be respectful and constructive.

## License

By contributing, you agree that your contributions will be licensed under the [MIT License](license.md).