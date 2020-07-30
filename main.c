#include <stdio.h>
#include <stdlib.h>

// #define DEBUG
#define MAX_LINE_LENGTH 1024
#define HISTORY_BUFFER_SIZE 100
#define TEXT_BUFFER_SIZE 100000
#define OFFSET_TOLLERANCE 1000

// ----- TYPES -----

typedef enum boolean {
    false, true
} t_boolean;

typedef struct data {
    char **text;
    int length;
} t_data;

typedef struct command {
    char type;
    int start;
    int end;
    t_data data;
    t_data prevData;
    struct command *next;
} t_command;

typedef struct text {
    char **lines;
    int numLines;
    int offset;
} t_text;

typedef struct history {
    // stack
    t_command *pastCommands;
    int numPastCommands;
    t_command *futureCommands;
    int numFutureCommands;
    t_boolean timeTravelMode;
    int commandsToTravel;
} t_history;

// ----- PROTOTYPES -----

// read command
t_command *readCommand();

int getCommandType(char *);

int readCommandStart(t_command, char *);

void readCommandStartAndEnd(t_command *, char *);

t_data readCommandData(t_command);

t_data getEmptyDataStruct();

// execute command
void executeCommand(t_command *, t_text *, t_history *);

void printCommand(t_command, t_text *);

void changeCommand(t_command *, t_text *);

void deleteCommand(t_command *, t_text *);

void undoCommand(t_text *, t_history *);

void redoCommand(t_text *, t_history *);

// update history
void createHistory(t_history *);

void updateHistory(t_history *, t_command *);

void checkForPastChanges(t_history *, t_command *);

void forgetFuture(t_history *);

void addNewEventToHistory(t_history *, t_command *);

void backToTheFuture(t_history *history, t_text *text);

void backToThePast(t_history *history, t_text *text);

void revertChange(t_command *command, t_text *text);

void swipeEventStack(t_command *, t_command **, t_command **);

void returnToStart(t_text *, t_history *);

// text manager
void createText(t_text *);

void createNewText(t_text **);

t_data readText(t_text *, int, int);

void writeText(t_text *, t_data *, int, int);

void rewriteText(t_text *, t_data, int, int);

void addTextInMiddle(t_text *, t_data, int, int);

void deleteTextLines(t_text *, t_command *);

void checkAndReallocText(t_text *, int);

void createSpaceInMiddleText(t_text *, int, int);

t_boolean checkIfExceedBufferSize(int, int);

void allocateBiggerTextArea(t_text *, int);

void writeDataToText(t_text *, t_data, int);

t_boolean isDataValidForWrite(t_text *, t_data, int);

void shiftText(t_text *, t_command *);

void readAndWriteText(t_text *, t_command *);

void shiftTextUp(t_text *, t_command *);

void shiftTextDown(t_text *, t_command *);

// utilities
char *readLine();

void printLine(char *);

int stringSize(char *string);

void swipeData(t_data *, t_data *);

// ----- READ COMMAND -----

t_command *readCommand() {
    t_command *command;
    char *line;

    command = malloc(sizeof(t_command));

    line = readLine();
    // 1. Read type
    command->type = getCommandType(line);

    // 2. Read interval
    // undo and redo do not have end
    command->end = 0;
    if (command->type == 'c' || command->type == 'd' || command->type == 'p')
        readCommandStartAndEnd(command, line);
    if (command->type == 'u' || command->type == 'r')
        command->start = readCommandStart(*command, line);

    // 3. Read data
    if (command->type == 'c')
        command->data = readCommandData(*command);
    else
        command->data = getEmptyDataStruct();

    // clear read line
    free(line);
    // initialize prevData
    command->prevData.text = NULL;
    command->prevData.length = 0;

    return command;
}

int getCommandType(char *line) {
    // command type is always the last char of the line
    return line[stringSize(line) - 1];
}

t_data getEmptyDataStruct() {
    t_data data;
    data.text = NULL;
    data.length = 0;
    return data;
}

t_data readCommandData(t_command command) {
    int numLines;
    char *line;
    t_data data;

    // start cannot be under 0
    if (command.start <= 0) {
        command.start = 1;
    }

    // num of lines is given by the difference between end and start
    numLines = command.end - command.start + 1;

    // allocate numLines strings
    data.text = malloc(sizeof(char *) * numLines + 1);
    // read lines
    for (int i = 0; i < numLines; i++) {
        line = readLine();
        data.text[i] = line;
    }

    // read last line with dot
    line = readLine();
    free(line);

    // save data length
    data.length = numLines;

    #ifdef DEBUG
        if(line[0] != '.')
        {
            printf("ERROR: change command not have a dot as last line\n");
        }
    #endif
    return data;
}

int readCommandStart(t_command command, char *line) {
    char numStr[MAX_LINE_LENGTH];
    // counter for line
    int i = 0;
    // counter for numStr
    int j = 0;

    // read start
    while (line[i] != command.type) {
        numStr[j] = line[i];
        i++;
        j++;
    }

    numStr[j] = '\0';

    return atoi(numStr);
}

void readCommandStartAndEnd(t_command *command, char *line) {
    char numStr[MAX_LINE_LENGTH];
    // counter for line
    int i = 0;
    // counter for numStr
    int j = 0;

    // read start
    while (line[i] != ',') {
        numStr[j] = line[i];
        i++;
        j++;
    }

    numStr[j] = '\0';
    command->start = atoi(numStr);

    j = 0;
    // curr char is ','
    i++;
    // read end
    while (line[i] != command->type) {
        numStr[j] = line[i];
        i++;
        j++;
    }
    numStr[j] = '\0';
    command->end = atoi(numStr);

    return;
}

// ----- EXECUTE COMMAND -----

void executeCommand(t_command *command, t_text *text, t_history *history) {
    if (history->timeTravelMode == true && command->type != 'r' && command->type != 'u') {
        if (history->commandsToTravel < 0) {
            undoCommand(text, history);
        } else if (history->commandsToTravel > 0) {
            redoCommand(text, history);
        }
        // otherwise nothing to do
    }
    switch (command->type) {
        case 'p':
            printCommand(*command, text);
            break;
        case 'c':
            changeCommand(command, text);
            break;
        case 'd':
            deleteCommand(command, text);
            break;
        case 'u':
            if (command->start > history->numPastCommands) {
                command->start = history->numPastCommands;
            }
            history->commandsToTravel -= command->start;
            history->numPastCommands -= command->start;
            history->numFutureCommands += command->start;
            history->timeTravelMode = true;
            break;
        case 'r':
            if (history->timeTravelMode = true) {
                if (command->start > history->numFutureCommands) {
                    command->start = history->numFutureCommands;
                }
                history->commandsToTravel += command->start;
                history->numPastCommands += command->start;
                history->numFutureCommands -= command->start;
            }
            break;
        #ifdef DEBUG
            default:
                printf("ERROR: cannot identify command type\n");
            break;
        #endif
    }
}

void printCommand(t_command command, t_text *text) {
    int numTextLinesToPrint = 0;
    t_data data;
    if (command.start == 0) {
        printf(".\n");
        command.start = 1;
    }
    // check if start is in text
    if (command.start > text->numLines) {
        for (int i = command.start; i < command.end + 1; i++)
            printf(".\n");
    } else {
        // check for overflow
        if (command.end > text->numLines) {
            numTextLinesToPrint = text->numLines - command.start + 1;
        } else {
            numTextLinesToPrint = command.end - command.start + 1;
        }

        for (int i = 0; i < numTextLinesToPrint; i++)
            printf("%s\n", text->lines[command.start - 1 + i + text->offset]);

        for (int i = text->numLines; i < command.end; i++)
            printf(".\n");

    }
}

void changeCommand(t_command *command, t_text *text) {
    // start cannot be equal or lower 0
    if (command->start <= 0) {
        command->start = 1;
    }
    // start cannot be greater than last line + 1
    // if(command -> start > text -> numLines + 1)
    //    return;

    readAndWriteText(text, command);
    return;
}

void deleteCommand(t_command *command, t_text *text) {
    // start cannot be equal or lower 0
    if (command->start <= 0) {
        command->start = 1;
    }
    // start cannot be greater than last line
    if(command -> start > text -> numLines)
    {
        return;
    }
    deleteTextLines(text, command);
    return;
}

void returnToStart(t_text *text, t_history *history)
{
    // move commands to futureStack
    for (int i = 0; i < history->commandsToTravel; i++) {
        swipeData(&(history->pastCommands->data), &(history->pastCommands->prevData));
        swipeEventStack(history->pastCommands, &history->pastCommands, &history->futureCommands);
    }
    // create new empty text
    createNewText(&text);
    history->commandsToTravel = 0;
    return;
}

void undoCommand(t_text *text, t_history *history) {
    // set as positive value
    history->commandsToTravel *= -1;

    // cannot undo 0 or lower
    // if(history -> commandsToTravel <= 0)
    //     return;

    // if after undo remain no commands, so simple go back to the start
    if(history->numPastCommands == 0) {
        returnToStart(text, history);
        return;
    }

    // undo commands
    for (int i = 0; i < history->commandsToTravel; i++) {
        // no others commands to undo, so exit
        // if(history -> pastCommands == NULL)
        // {
        //     break;
        // }
        backToThePast(history, text);
    }
    history->commandsToTravel = 0;
    return;
}

void redoCommand(t_text *text, t_history *history) {
    // cannot undo 0 or lower
    // if(history -> commandsToTravel <= 0)
    //     return;
    // undo commands
    for (int i = 0; i < history->commandsToTravel; i++) {
        // no others commands to undo, so exit
        // if(history -> futureCommands == NULL)
        // {
        //     break;
        // }
        backToTheFuture(history, text);
    }
    history->commandsToTravel = 0;
    return;
}

// ----- UPDATE HISTORY -----

void createHistory(t_history *history) {
    history->commandsToTravel = 0;
    history->timeTravelMode = false;
    history->futureCommands = NULL;
    history->pastCommands = NULL;
    history->numPastCommands = 0;
    history->numFutureCommands = 0;
    return;
}

void updateHistory(t_history *history, t_command *command) {
    checkForPastChanges(history, command);
    addNewEventToHistory(history, command);
    return;
}

void checkForPastChanges(t_history *history, t_command *command) {
    // only when true check if future is change
    // than you cannot turn back to your dimension
    if (history->timeTravelMode == true) {
        // only while doing undo and redo you can return in the future
        // modify the past creates a new future
        if (command->type == 'c' || command->type == 'd') {
            history->timeTravelMode = false;
            forgetFuture(history);
        }
    }
    return;
}

void forgetFuture(t_history *history) {
    t_command *command, *app;
    command = history->futureCommands;
    while (command != NULL) {
        app = command;
        command = command->next;
        free(app);
    }
    history->futureCommands = NULL;
    history->numFutureCommands = 0;
    return;
}

void addNewEventToHistory(t_history *history, t_command *command) {
    t_command *newHead;

    // only change and delete commands should be saved
    if (command->type == 'c' || command->type == 'd') {
        newHead = command;
        newHead->next = history->pastCommands;
        history->pastCommands = newHead;
        history->numPastCommands++;
    } else {
        free(command);
    }
    return;
}

void backToThePast(t_history *history, t_text *text) {
    t_command *command, *app;
    char **strApp;

    // no commands to do
    // if(history -> pastCommands == NULL)
    //     return;

    command = history->pastCommands;
    // change command to redo
    if (command->type == 'c') {
        revertChange(command, text);
    }
        // delete command to redo
    else if (command->type == 'd') {
        if (command->prevData.text != NULL)
            addTextInMiddle(text, command->prevData, command->start, command->end);
    }
    #ifdef DEBUG
    else
    {
        printf("ERROR: try to time travel with a bad command\n");
    }
    #endif

    swipeEventStack(command, &history->pastCommands, &history->futureCommands);
    return;
}

void backToTheFuture(t_history *history, t_text *text) {
    t_command *command, *app;

    // no commands to do
    // if(history -> futureCommands == NULL)
    //     return;

    command = history->futureCommands;
    // change command to redo
    if (command->type == 'c') {
        revertChange(command, text);
    }
        // delete command to redo
    else if (command->type == 'd') {
        deleteCommand(command, text);
    }
    #ifdef DEBUG
    else
    {
        printf("ERROR: try to time travel with a bad command\n");
    }
    #endif

    // command go to other stack
    swipeEventStack(command, &history->futureCommands, &history->pastCommands);
    return;
}

void revertChange(t_command *command, t_text *text) {

    // restore prev data
    if (command->prevData.text == NULL && command->start == 1) {
        // create new empty text
        createNewText(&text);
    } else {
        rewriteText(text, command->prevData, command->start, command->end);
    }

    // swap
    swipeData(&(command->prevData), &(command->data));
    return;
}

void swipeEventStack(t_command *command, t_command **old, t_command **new) {
    t_command *app;
    // command go to other stack, so set next as head of commands stack
    app = command->next;
    if (*new == NULL) {
        command->next = NULL;
    } else {
        command->next = *new;
    }
    *new = command;
    // set command as head for the second commands stack
    *old = app;
    return;
}


// ----- TEXT MANAGER -----

void createText(t_text *text) {
    text->numLines = 0;
    text->offset = OFFSET_TOLLERANCE;
    text->lines = malloc(sizeof(char *) * TEXT_BUFFER_SIZE);
    return;
}

void createNewText(t_text **text)
{
    (*text)->numLines = 0;
    (*text)->offset = OFFSET_TOLLERANCE;
    free((*text)->lines);
    (*text)->lines = malloc(sizeof(char *) * TEXT_BUFFER_SIZE);
    return;
}

t_data readText(t_text *text, int start, int end) {
    t_data data;
    int numLinesToRead;

    data.text = NULL;
    data.length = 0;

    // end cannot be greater then numLines
    if (end > text->numLines)
        end = text->numLines;

    numLinesToRead = end - start + 1;

    // check if start not exceed numLines, otherwise no data to read
    if (start > text->numLines)
        return data;

    // check if end not exceed numLines, otherwise end must be decrease to it
    if (end > text->numLines)
        end = text->numLines;

    // allocate an array of strings
    // 1,3c -> 3 - 1 + 1 = 3 lines to write
    data.text = malloc(sizeof(char *) * numLinesToRead);

    // read lines
    for (int i = 0; i < numLinesToRead; i++) {
        data.text[i] = text->lines[start + i - 1 + text->offset];
    }

    // set text length
    data.length = numLinesToRead;

    return data;
}

void readAndWriteText(t_text *text, t_command *command)
{
    int end = command->end;
    int numLinesToRead = 0;
    int numLinesToAppend = 0;
    int numCurrLine = 0;
    int numLinesWritten = 0;

    command->prevData.text = NULL;
    command->prevData.length = 0;

    // check data
    if (!isDataValidForWrite(text, command->data, command->start))
        return;

    // start cannot be less or equal than 0
    // if (command->start <= 0)
    //     command->start = 1;

    // check if end not exceed numLines, otherwise end must be decrease to it
    if (end > text->numLines)
        end = text->numLines;

    // allocate an array of strings
    // 1,3c -> 3 - 1 + 1 = 3 lines to write
    numLinesToRead = end - command->start + 1;

    if(numLinesToRead > 0)
    {
        command->prevData.text = malloc(sizeof(char *) * numLinesToRead);

        // read and write lines
        for (int i = 0; i < numLinesToRead; i++) {
            numCurrLine = command->start + i - 1 + text->offset;
            // save prevData
            command->prevData.text[i] = text->lines[numCurrLine];
            // update text
            text->lines[numCurrLine] = command->data.text[i];
            numLinesWritten++;
        }
    }

    // write append lines
    numLinesToAppend = command->data.length - numLinesToRead;
    for (int i = 0; i < numLinesToAppend; i++) {
        numCurrLine = command->start + i - 1 + text->offset + numLinesWritten;
        // update text
        text->lines[numCurrLine] = command->data.text[i + numLinesWritten];
    }

    // set prev data text length
    command->prevData.length = numLinesToRead;

    // check if this write append new text
    if (command->end > text->numLines) {
        // check for reallocation
        // checkAndReallocText(text, end);
        // update text num lines
        text->numLines = command->end;
    }

    return;
}


void deleteTextLines(t_text *text, t_command *command) {
    int start = command->start;
    int numLinesToDelete = 0;

    // initialize data
    command->prevData.text = NULL;
    command->prevData.length = 0;

    // start cannot be greater than text num lines
    if (start > text->numLines)
        return;

    // start cannot be less or equal than 0
    if (start <= 0)
        start = 1;

    // cannot delete a num lines lower or equal than 0
    numLinesToDelete = command->end - start + 1;
    if (numLinesToDelete <= 0)
        return;

    // if end is bigger or equal text num lines
    // than simply decrease text num lines to overwrite them
    if (command->end >= text->numLines) {
        // only save data
        command->prevData = readText(text, command->start, text->numLines);
        text->numLines = start - 1;
        return;
    }

    // shift lines
    shiftText(text, command);

    // save new num lines
    text->numLines = text->numLines - numLinesToDelete;

    return;
}

void writeText(t_text *text, t_data *data, int start, int end) {
    // check data
    if (!isDataValidForWrite(text, *data, start))
        return;

    // start cannot be less or equal than 0
    if (start <= 0)
        start = 1;

    // check if this write will append new text
    if (end > text->numLines) {
        // check for reallocation
        // checkAndReallocText(text, end);
        // update text num lines
        text->numLines = end;
    }

    // write lines
    writeDataToText(text, *data, start);

    return;
}

void rewriteText(t_text *text, t_data data, int start, int end) {
    // check data
    if (!isDataValidForWrite(text, data, start))
        return;

    // start cannot be less or equal than 0
    if (start <= 0)
        start = 1;

    // check if this write will append new text
    if (end > text->numLines) {
        // check for reallocation
        // checkAndReallocText(text, end);
        // update text num lines
        text->numLines = end;
    }
        // check if this command added new lines before
    else if (end == text->numLines && data.length < end - start + 1) {
        text->numLines -= end - data.length;
    }

    // write lines
    writeDataToText(text, data, start);

    return;
}

void addTextInMiddle(t_text *text, t_data data, int start, int end) {
    // check data
    if (!isDataValidForWrite(text, data, start))
        return;

    // create space
    createSpaceInMiddleText(text, start, data.length);

    // write lines
    writeDataToText(text, data, start);

    // save new num lines
    text->numLines = text->numLines + data.length;
    return;
}

void shiftTextDown(t_text *text, t_command *command) {
    int numLinesToRead = command->end - command->start + 1;
    int numLinesRead = 0;
    int departure;
    int arrive;
    int numLinesToShift;

    // shift and read
    for (int i = 0; i < numLinesToRead; i++) {
        departure = command -> start - 2 - i + text->offset;
        arrive = command -> end - 1 - i + text->offset;

        // read prevData
        command->prevData.text[numLinesToRead - 1 - i] = text->lines[arrive];
        // overwrite lines from start to end
        text->lines[arrive] = text->lines[departure];
        numLinesRead++;
    }
    // only shift
    numLinesToShift = command->start - numLinesRead - 1;
    for (int i = 0; i < numLinesToShift; i++) {
        departure = command->start - 2 - i + text->offset - numLinesRead;
        arrive = command->end - 1 - i + text->offset - numLinesRead;
        // overwrite lines from start to end
        text->lines[arrive] = text->lines[departure];
    }
    text->offset += command->end - command->start + 1;

    return;
}

void shiftTextUp(t_text *text, t_command *command) {
    int numLinesToRead = command->end - command->start + 1;
    int numLinesRead = 0;
    int departure;
    int arrive;
    int numLinesToShift;

    // shift and read
    for (int i = 0; i < numLinesToRead; i++) {
        departure = command -> end + i + text->offset;
        arrive = command -> start + i - 1 + text->offset;

        // read prevData
        command->prevData.text[i] = text->lines[arrive];
        // overwrite lines from start to end
        text->lines[arrive] = text->lines[departure];
        numLinesRead++;
    }
    // only shift
    numLinesToShift = text->numLines - command -> end - numLinesRead;
    for (int i = 0; i < numLinesToShift; i++) {
        departure = command -> end + i + text->offset + numLinesRead;
        arrive = command -> start + i - 1 + text->offset + numLinesRead;
        // overwrite lines from start to end
        text->lines[arrive] = text->lines[departure];
    }

    return;
}

void shiftText(t_text *text, t_command *command) {
    int middle;
    int start = command -> start;
    int end = command -> end;
    int numLinesToRead = command->end - command->start + 1;

    // define middle element index in range
    middle = start + ((end - start + 1) / 2);

    // special cases
    // start equal to 1 nothing to do, is enough to increase offset
    if (start == 1) {
        // only save data
        command->prevData = readText(text, command->start, command->end);
        text->offset += end;
        return;
    }

    command->prevData.text = malloc(sizeof(char *) * numLinesToRead);
    command->prevData.length = numLinesToRead;

    // check if middle is in first half or in the second
    if (middle < text->numLines / 2) {
        shiftTextDown(text, command);
    } else {
        shiftTextUp(text, command);
    }
    return;
}

t_boolean isDataValidForWrite(t_text *text, t_data data, int start) {
    // start cannot be greater than text num lines
    if (start > text->numLines + 1)
        return false;

    // cannot have a num lines lower or equal than 0
    if (data.length <= 0)
        return false;

    return true;
}

// checkAndReallocText: check if exceed allocated memory, than realloc
void checkAndReallocText(t_text *text, int newLastLine) {
    // check if exceed allocated memory, otherwise realloc
    if (checkIfExceedBufferSize(text->numLines + text->offset, newLastLine + text->offset)) {
        // reallocate a new area with a more buffer size
        allocateBiggerTextArea(text, newLastLine);
    }
}

// createSpaceInMiddleText: move text lines to create space
void createSpaceInMiddleText(t_text *text, int start, int dataLength) {
    int newExtremeValue;
    int numLinesToMove;
    int middle;

    // define middle element index in range
    middle = start + dataLength / 2;

    // check if middle is in first half or in the second
    if (middle < text->numLines / 2) {
        numLinesToMove = start + dataLength - 1;
        newExtremeValue = text->offset - dataLength;
        // newExtremeValue = text -> offset + data.length;
        for (int i = 0; i < numLinesToMove; i++) {
            // overwrite lines from start to end
            text->lines[newExtremeValue + i] = text->lines[text->offset + i];
        }
        text->offset -= dataLength;
    } else {
        numLinesToMove = text->numLines - start + 1;
        newExtremeValue = text->numLines + dataLength;
        for (int i = 0; i < numLinesToMove; i++) {
            // overwrite lines from old to new end
            text->lines[newExtremeValue - i - 1 + text->offset] = text->lines[text->numLines - i - 1 + text->offset];
        }
    }

    return;
}

void writeDataToText(t_text *text, t_data data, int start) {
    int numCurrLine;
    for (int i = 0; i < data.length; i++) {
        numCurrLine = start + i - 1 + text->offset;
        text->lines[numCurrLine] = data.text[i];
    }
    return;
}

t_boolean checkIfExceedBufferSize(int currLinesAllocated, int newNumLinesAllocated) {
    return ((newNumLinesAllocated / TEXT_BUFFER_SIZE) > (currLinesAllocated / TEXT_BUFFER_SIZE + 1));
}

void allocateBiggerTextArea(t_text *actualText, int newLastLine) {
    int buffersToAllocate = (newLastLine + actualText->offset) / TEXT_BUFFER_SIZE + 1;
    actualText->lines = realloc(actualText->lines, sizeof(char *) * buffersToAllocate * TEXT_BUFFER_SIZE);
    // check for realloc errors
    if (actualText->lines == NULL) {
        abort();
    }
}

// ----- UTILITIES FUNCTIONS -----

int stringSize(char *string) {
    int count = 0;
    while (string[count] != '\0') count++;
    return count;
}

char *readLine() {
    int i;
    char c;
    char *line = malloc(sizeof(char) * (MAX_LINE_LENGTH + 1));

    c = getchar();
    i = 0;

    while (c != '\n') {
        line[i] = c;
        i++;

        c = getchar();
    }
    line[i] = '\0';
    i++;

    line = realloc(line, sizeof(char) * (i + 1));

    return line;
}

void printLine(char *line) {
    int i = 0;
    while (line[i] != '\0') {
        putchar(line[i]);
        i++;
    }
    putchar('\n');
}

void swipeData(t_data *data1, t_data *data2)
{
    t_data app;
    app = *data1;
    *data1 = *data2;
    *data2 = app;
    return;
}

// ----- MAIN -----

int main() {
    t_text text;
    t_history history;
    t_command *command;

    createText(&text);
    createHistory(&history);

    /*
        Execution process:
        1. read command
        2. execute command
        3. update history
    */

    command = readCommand();

    while (command->type != 'q') {
        executeCommand(command, &text, &history);
        updateHistory(&history, command);

        command = readCommand();
        #ifdef DEBUG
                printf("Command type: %c\n", command.type);
                printf("Command start: %d\n", command.start);
                printf("Command end: %d\n", command.end);
                printf("Text length: %d\n", text.numLines);
        #endif
    }

    return 0;
}