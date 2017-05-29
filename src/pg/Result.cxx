/*
 * author: Max Kellermann <mk@cm4all.com>
 */

#include "Result.hxx"

namespace Pg {

std::string
Result::GetOnlyStringChecked() const
{
    if (!IsQuerySuccessful() || GetRowCount() == 0)
        return std::string();

    const char *value = GetValue(0, 0);
    if (value == nullptr)
        return std::string();

    return value;
}

} /* namespace Pg */
