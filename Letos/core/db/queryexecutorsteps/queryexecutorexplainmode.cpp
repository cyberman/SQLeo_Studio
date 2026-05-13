#include "queryexecutorexplainmode.h"
#include "services/config.h"

bool QueryExecutorExplainMode::exec()
{
    if (context->explainMode < 0)
        return true; // explain mode disabled

    Cfg::ExplainQueryMode mode = static_cast<Cfg::ExplainQueryMode>(context->explainMode);

    SqliteQueryPtr lastQuery = context->parsedQueries.last();

    if (!lastQuery)
        return true;

    // If last query wasn't in explain mode, switch it on
    if (!lastQuery->explain)
    {
        if (mode == Cfg::ExplainQueryMode::QUERY_PLAN)
        {
            lastQuery->queryPlan = true;
            lastQuery->tokens.prepend(TokenPtr::create(Token::SPACE, " "));
            lastQuery->tokens.prepend(TokenPtr::create(Token::KEYWORD, "PLAN"));
            lastQuery->tokens.prepend(TokenPtr::create(Token::SPACE, " "));
            lastQuery->tokens.prepend(TokenPtr::create(Token::KEYWORD, "QUERY"));
        }
        lastQuery->explain = true;
        lastQuery->tokens.prepend(TokenPtr::create(Token::SPACE, " "));
        lastQuery->tokens.prepend(TokenPtr::create(Token::KEYWORD, "EXPLAIN"));
    }

    // Limit queries to only last one
    context->parsedQueries.clear();
    context->parsedQueries << lastQuery;

    updateQueries();

    return true;
}
