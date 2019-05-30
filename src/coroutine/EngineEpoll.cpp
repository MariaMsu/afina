#include <afina/coroutine/EngineEpoll.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

namespace Afina {
namespace Coroutine {

// Save stack of the current coroutine in the given context
void Engine::Store(context &ctx) {
    char current_addr;
    ctx.Hight = ctx.Low = StackBottom;
//  для стека, растущего в любую сторону
    if (&current_addr > StackBottom) {
        ctx.Hight = &current_addr;
    } else {
        ctx.Low = &current_addr;
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
    char current_addr;
    if ((ctx.Low <= &current_addr) && (&current_addr <= ctx.Hight)) {
        Restore(ctx);
    }

    char *&buffer = std::get<0>(ctx.Stack);
    auto size = ctx.Hight - ctx.Low;
    memcpy(ctx.Low, buffer, size);
    longjmp(ctx.Environment, 1);
}


// Gives up current routine execution and let engine to schedule other one
void Engine::yield() {
    context *candidate = alive;

//  берем следующую корутину, если такая имеется
    if (candidate && (candidate == cur_routine)) {
        candidate = candidate->next;
    }

//  ставим на выполнение новую корутину всесто старой
    if (candidate) {
        Enter(*candidate);
    }
}

// Gives up current routine execution and let engine to schedule given
void Engine::sched(void *routine_) {
    if (cur_routine == routine_) {
        return;
    }

    if (routine_) {
        Enter(*(static_cast<context *>(routine_)));
    } else {
        yield();
    }
}

// Suspend current coroutine execution and execute given context
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

void Engine::Move2alive(context * const &routine){
    if (routine->prev != nullptr) {
        routine->prev->next = routine->next;
    }
    if (routine->next != nullptr) {
        routine->next->prev = routine->prev;
    }

    if (alive == routine) {
        alive = alive->next;
    }

    routine->next = blocked;
    blocked = routine;
    if (routine->next != nullptr) {
        routine->next->prev = routine;
    }
}

void Engine::Move2blocked(context * const &routine){
    if (routine->prev != nullptr) {
        routine->prev->next = routine->next;
    }
    if (routine->next != nullptr) {
        routine->next->prev = routine->prev;
    }

    if (blocked == routine) {
        blocked = blocked->next;
    }

    routine->next = alive;
    alive = routine;
    if (routine->next != nullptr) {
        routine->next->prev = routine;
    }
}

} // namespace Coroutine
} // namespace Afina