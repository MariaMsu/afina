#include <afina/coroutine/Engine.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

// Save stack of the current coroutine in the given context
void Engine::Store(context &ctx) {
    char stack_head;
    ctx.Hight = ctx.Low = StackBottom;
//    для стека, растущего в любую сторону
    if (&stack_head > StackBottom) {
        ctx.Hight = &stack_head;
    } else {
        ctx.Low = &stack_head;
    }

    char *&buffer = std::get<0>(ctx.Stack);
    uint32_t &required_size = std::get<1>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;

    if (required_size < size) {
        delete[] buffer;
        buffer = new char[size];
        required_size = size;
    }
    memcpy(buffer, ctx.Low, size);
}

//  Restore stack of the given context and pass control to coroutine
void Engine::Restore(context &ctx) {
    char stack_head;
    if ((ctx.Low <= &stack_head) && (&stack_head <= ctx.Hight)) {
        Restore(ctx);
    }

    char *&buffer = std::get<0>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    memcpy(ctx.Low, buffer, size);
    longjmp(ctx.Environment, 1);
}

void Engine::yield() {
    context *candidate = alive;

    if (candidate && (candidate == cur_routine)) {
        candidate = candidate->next;
    }

    if (candidate) {
        Enter(*candidate);
    }
}

void Engine::sched(void *routine_) {
    if (cur_routine == routine_) {
        return;
    }

    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    }
    else {
        yield();
    }
}

void Engine::Enter(context &ctx) {
    if (cur_routine && cur_routine != idle_ctx) {
        if (setjmp(cur_routine->Environment) > 0) {
            return;
        }
        Store(*cur_routine);
    }

    cur_routine = &ctx;
    Restore(ctx);
}


} // namespace Coroutine
} // namespace Afina
