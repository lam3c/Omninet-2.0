
#include "authenticationdetector.h"

#include <QRegularExpression>

AuthenticationInfo AuthenticationDetector::inspect(const QString &html)
{
    AuthenticationInfo result;

    // Find HTML forms in the router's login page.
    QRegularExpression formRegex(
        R"(<form\b([^>]*)>([\s\S]*?)</form>)",
        QRegularExpression::CaseInsensitiveOption
        );

    QRegularExpressionMatchIterator forms =
        formRegex.globalMatch(html);

    while (forms.hasNext())
    {
        QRegularExpressionMatch formMatch = forms.next();

        const QString attributes = formMatch.captured(1);
        const QString contents = formMatch.captured(2);

        // Only consider forms containing a password field.
        QRegularExpression passwordRegex(
            R"(<input\b[^>]*type\s*=\s*["']?password["']?[^>]*>)",
            QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatch passwordMatch =
            passwordRegex.match(contents);

        if (!passwordMatch.hasMatch())
            continue;

        result.formFound = true;

        // Extract the form submission destination.
        QRegularExpression actionRegex(
            R"(\baction\s*=\s*["']([^"']*)["'])",
            QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatch actionMatch =
            actionRegex.match(attributes);

        if (actionMatch.hasMatch())
            result.formAction = actionMatch.captured(1);

        // Extract the submission method.
        QRegularExpression methodRegex(
            R"(\bmethod\s*=\s*["']([^"']*)["'])",
            QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatch methodMatch =
            methodRegex.match(attributes);

        result.formMethod = methodMatch.hasMatch()
                                ? methodMatch.captured(1).toUpper()
                                : "GET";

        // Extract the password input's name.
        QRegularExpression nameRegex(
            R"(\bname\s*=\s*["']([^"']+)["'])",
            QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatch passwordNameMatch =
            nameRegex.match(passwordMatch.captured(0));

        if (passwordNameMatch.hasMatch())
            result.passwordField = passwordNameMatch.captured(1);

        // Inspect all inputs to identify a username field.
        QRegularExpression inputRegex(
            R"(<input\b[^>]*>)",
            QRegularExpression::CaseInsensitiveOption
            );

        QRegularExpressionMatchIterator inputs =
            inputRegex.globalMatch(contents);

        while (inputs.hasNext())
        {
            const QString input = inputs.next().captured(0);

            QRegularExpression inputTypeRegex(
                R"(\btype\s*=\s*["']?([^"'\s>]+))",
                QRegularExpression::CaseInsensitiveOption
                );

            QRegularExpressionMatch typeMatch =
                inputTypeRegex.match(input);

            const QString inputType = typeMatch.hasMatch()
                                          ? typeMatch.captured(1).toLower()
                                          : "text";

            QRegularExpressionMatch inputNameMatch =
                nameRegex.match(input);

            if (!inputNameMatch.hasMatch())
                continue;

            const QString fieldName = inputNameMatch.captured(1);

            if (inputType == "hidden")
            {
                result.hiddenFields.append(fieldName);
                continue;
            }

            if (inputType == "password")
                continue;

            const QString lowerName = fieldName.toLower();

            const bool looksLikeUsername =
                lowerName.contains("user") ||
                lowerName.contains("login") ||
                lowerName.contains("account") ||
                lowerName == "uname" ||
                lowerName == "username";

            if (looksLikeUsername)
                result.usernameField = fieldName;
        }

        // Classify the login form.
        if (!result.passwordField.isEmpty())
        {
            if (!result.usernameField.isEmpty())
            {
                result.type =
                    AuthenticationType::UsernameAndPassword;

                result.explanation =
                    "The login form contains username and password fields.";
            }
            else
            {
                result.type = AuthenticationType::PasswordOnly;

                result.explanation =
                    "The login form contains a password field but no "
                    "recognised username field.";
            }
        }
        else
        {
            result.type = AuthenticationType::Unsupported;
            result.explanation =
                "The password field does not have a recognised name.";
        }

        return result;
    }

    result.explanation =
        "No standard HTML login form with a password field was found.";

    return result;
}
