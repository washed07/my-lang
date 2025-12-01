Diagnostic(UnusedVariableWarning, 
            DiagnosticLevel::Warning,
            DiagnosticKind::Semantic, 
            "Unused variable '%0'.",
            "The variable '%0' is declared but never used.")
Diagnostic(DeprecatedFeatureWarning, 
            DiagnosticLevel::Warning,
            DiagnosticKind::Semantic,
            "Deprecated feature '%0' used.",
            "The feature '%0' is deprecated and should not be used.")
Diagnostic(UnreachableCodeWarning, 
            DiagnosticLevel::Warning,
            DiagnosticKind::Semantic,
            "Unreachable code detected.",
            "Code detected that will never be executed.")
Diagnostic(PossibleNullDereferenceWarning, 
            DiagnosticLevel::Warning,
            DiagnosticKind::Semantic,
            "Possible null pointer dereference.",
            "There is a possibility of dereferencing a null pointer.")
Diagnostic(ImplicitConversionWarning, 
            DiagnosticLevel::Warning,
            DiagnosticKind::Type,
            "Implicit conversion from '%0' to '%1'.", 
            "An implicit conversion from type '%0' to type '%1' may lead to data loss or unexpected behavior.")