Diagnostic(FileNotFoundError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System,
            "File not found.", 
            "The file '%0' could not be found.")
Diagnostic(PathNotFoundError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Path not found.", 
            "The path '%0' could not be found.")
Diagnostic(AccessDeniedError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Access Denied.", 
            "Access to the resource '%0' was denied.")
Diagnostic(MemoryOverflowError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Memory overflow.", 
            "Memory overflow occurred during operation.")
Diagnostic(MemoryUnderflowError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Memory underflow.", 
            "Memory underflow occurred during operation.")
Diagnostic(StackOverflowError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Stack overflow.", 
            "Stack overflow occurred during operation.")
Diagnostic(StackUnderflowError, 
            DiagnosticLevel::Error,
            DiagnosticKind::System, 
            "Stack underflow.", 
            "Stack underflow occurred during operation.")
Diagnostic(UnterminatedCommentError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Lexical, 
            "Unterminated comment.", 
            "The comment was not properly terminated.")
Diagnostic(UnterminatedCharacterLiteralError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Lexical, 
            "Unterminated character literal.", 
            "The character literal is missing a closing quote.")
Diagnostic(InvalidCharacterLiteralError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Lexical, 
            "Invalid character literal.", 
            "The character literal is invalid.")
Diagnostic(UnterminatedStringLiteralError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Lexical, 
            "Unterminated string literal.", 
            "The string literal is missing a closing quote.")
Diagnostic(InvalidStringLiteralError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Lexical, 
            "Invalid string literal.", 
            "The string literal is invalid.")
Diagnostic(UnexpectedEofError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Unexpected end of file.", 
            "The end of file was reached unexpectedly.")
Diagnostic(UnexpectedValueError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Unexpected value.", 
            "Expected '%0', but found '%1'.")
Diagnostic(InvalidSyntaxError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Invalid syntax.", 
            "The syntax '%0' is invalid.")
Diagnostic(InvalidStatementError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Invalid statement.", 
            "The statement '%0' is invalid.")
Diagnostic(InvalidExpressionError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Invalid expression.", 
            "The expression '%0' is invalid.")
Diagnostic(InvalidDeclarationError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Syntax, 
            "Invalid declaration.", 
            "The declaration '%0' is invalid.")
Diagnostic(InvalidConditionalError,
            DiagnosticLevel::Error, 
            DiagnosticKind::Syntax, 
            "Invalid conditional.", 
            "The conditional '%0' is invalid.")
Diagnostic(TypeMismatchError, 
            DiagnosticLevel::Error,
            DiagnosticKind::Semantic, 
            "Type mismatch.", 
            "Expected type of '%0', but found type of '%1'.")