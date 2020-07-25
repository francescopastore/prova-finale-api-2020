#include <stdio.h>
#include <stdlib.h>

// #define DEBUG
#define MAX_LINE_LENGTH 1024
#define HISTORY_BUFFER_SIZE 100
#define TEXT_BUFFER_SIZE 1024

// ----- TYPES -----

typedef enum boolean { false, true } t_boolean;

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
} t_text;

typedef struct history {
    // stack
    t_command *pastCommands;
    t_command *futureCommands;
    t_boolean timeTravelMode;
} t_history;

// ----- PROTOTYPES -----

// read command
t_command* readCommand();
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
void undoCommand(t_command *, t_text *, t_history *);
void redoCommand(t_command *, t_text *, t_history *);

// update history
t_history createHistory();
void updateHistory(t_history *, t_command *);
void updateTimeTravelMode(t_history *, t_command *);
void forgetFuture(t_history *);
void addNewEventToHistory(t_history *, t_command *);
void backToTheFuture(t_history *history, t_text *text);
void backToThePast(t_history *history, t_text *text);
void revertChange(t_command *command, t_text *text);

// text manager
t_text createText();
t_data readText(t_text *, int, int);
void writeText(t_text *, t_data, int, int);
void deleteTextLines(t_text *, int, int);
void rewriteText(t_text *, t_data, int, int);
void addTextInMiddle(t_text *, t_data, int, int);
void checkAndReallocText(t_text *, int);
void createSpaceInMiddleText(t_text *, int, int);
t_boolean checkIfExceedBufferSize(int, int);
char **allocateBiggerTextArea(t_text *, int);
void writeDataToText(t_text *, t_data, int);
t_boolean isDataValidForWrite(t_text *, t_data, int);
void shiftText(t_text *, int, int);

// utilities
char* readLine();
void printLine(char *);
int stringSize(char* string);

// ----- READ COMMAND -----

t_command *readCommand()
{
    t_command *command;
    char *line;

    command = malloc(sizeof(t_command));

    line = readLine();
    // 1. Read type
    command -> type = getCommandType(line);

    // 2. Read interval
    // undo and redo do not have end
    command -> end = 0;
    if(command -> type == 'c' || command -> type == 'd' || command -> type == 'p')
        readCommandStartAndEnd(command, line);
    if(command -> type == 'u' || command -> type == 'r')
        command -> start = readCommandStart(*command, line);

    // 3. Read data
    if(command -> type == 'c')
        command -> data = readCommandData(*command);
    else
        command -> data = getEmptyDataStruct();

    // clear read line
    free(line);
    // initialize prevData
    command -> prevData.text = NULL;
    command -> prevData.length = 0;
    return command;
}

int getCommandType(char *line)
{
    // command type is always the last char of the line
    return line[stringSize(line) - 1];
}

t_data getEmptyDataStruct()
{
    t_data data;
    data.text = NULL;
    data.length = 0;
    return data;
}

t_data readCommandData(t_command command)
{
    int numLines;
    char *line;
    t_data data;

    // start cannot be under 0
    if(command.start <= 0)
    {
        command.start = 1;
    }

    // num of lines is given by the difference between end and start
    numLines = command.end - command.start + 1;

    // allocate numLines strings
    data.text = malloc(sizeof(char *) * numLines + 1);
    // read lines
    for(int i = 0; i < numLines; i++)
    {
        line = readLine();
        data.text[i] = line;
    }

    // read last line with dot
    line = readLine();

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

int readCommandStart(t_command command, char *line)
{
    char numStr[MAX_LINE_LENGTH];
    // counter for line
    int i = 0;
    // counter for numStr
    int j = 0;

    // read start
    while(line[i] != command.type)
    {
        numStr[j] = line[i];
        i++;
        j++;
    }

    numStr[j] = '\0';

    return atoi(numStr);
}

void readCommandStartAndEnd(t_command *command, char *line)
{
    char numStr[MAX_LINE_LENGTH];
    // counter for line
    int i = 0;
    // counter for numStr
    int j = 0;

    // read start
    while(line[i] != ',')
    {
        numStr[j] = line[i];
        i++;
        j++;
    }

    numStr[j] = '\0';
    command -> start = atoi(numStr);

    j = 0;
    // curr char is ','
    i++;
    // read end
    while(line[i] != command -> type)
    {
        numStr[j] = line[i];
        i++;
        j++;
    }
    numStr[j] = '\0';
    command -> end = atoi(numStr);

    return;
}

// ----- EXECUTE COMMAND -----

void executeCommand(t_command *command, t_text *text, t_history *history)
{
    switch(command -> type)
    {
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
            undoCommand(command, text, history);
        break;
        case 'r':
            redoCommand(command, text, history);
        break;
        #ifdef DEBUG
        default:
            printf("ERROR: cannot identify command type\n");
        break;
        #endif
    }
}

void printCommand(t_command command, t_text *text)
{
    int numTextLinesToPrint = 0;
    if(command.start == 0)
    {
        printf(".\n");
        command.start = 1;
    }
    // check if start is in text
    if(command.start > text -> numLines)
    {
        for(int i = command.start; i < command.end + 1; i++)
            printf(".\n");
    }
    else
    {
        // check for overflow
        if(command.end > text -> numLines)
        {
            numTextLinesToPrint = text -> numLines - command.start + 1;
        }
        else
        {
            numTextLinesToPrint = command.end - command.start + 1;
        }

        for(int i = 0; i < numTextLinesToPrint; i++)
            printf("%s\n", text -> lines[i + command.start - 1]);

        for(int i = text -> numLines; i < command.end; i++)
            printf(".\n");
    }
}

void changeCommand(t_command *command, t_text *text)
{
    // start cannot be equal or lower 0
    if(command -> start <= 0)
    {
        command -> start = 1;
    }
    // start cannot be greater than last line + 1
    if(command -> start > text -> numLines + 1)
        return;

    command -> prevData = readText(text, command -> start, command -> end);
    writeText(text, command -> data, command -> start, command -> end);
    return;
}

void deleteCommand(t_command *command, t_text *text)
{
    // start cannot be equal or lower 0
    if(command -> start <= 0)
    {
        command -> start = 1;
    }
    // start cannot be greater than last line
    if(command -> start > text -> numLines)
        return;

    command -> prevData = readText(text, command -> start, command -> end);
    deleteTextLines(text, command -> start, command -> end);
    return;
}

void undoCommand(t_command *command, t_text *text, t_history *history)
{
    // cannot undo 0 or lower
    if(command -> start <= 0)
        return;
    // undo commands
    for(int i = 0; i < command -> start; i++)
    {
        // no others commands to undo, so exit
        if(history -> pastCommands == NULL)
        {
            break;
        }
        backToThePast(history, text);
    }
    return;
}

void redoCommand(t_command *command, t_text *text, t_history *history)
{
    // cannot undo 0 or lower
    if(command -> start <= 0)
        return;
    // undo commands
    for(int i = 0; i < command -> start; i++)
    {
        // no others commands to undo, so exit
        if(history -> futureCommands == NULL)
        {
            break;
        }
        backToTheFuture(history, text);
    }
    return;
}

// ----- UPDATE HISTORY -----

t_history createHistory()
{
    t_history history;
    history.timeTravelMode = 0;
    history.futureCommands = NULL;
    history.pastCommands = NULL;
    return history;
}

void updateHistory(t_history *history, t_command *command)
{
    updateTimeTravelMode(history, command);
    addNewEventToHistory(history, command);
    return;
}

void updateTimeTravelMode(t_history *history, t_command *command)
{
    // only when true check if future is change
    // than you cannot turn back to your dimension
    if(history -> timeTravelMode == true)
    {
        // only while doing undo and redo you can return in the future
        if(command -> type == 'c' || command -> type == 'd')
        {
            history -> timeTravelMode = false;
            forgetFuture(history);
        }
    }
    return;
}

void forgetFuture(t_history *history)
{
    t_command *command, *app;
    command = history -> futureCommands;
    while(command != NULL)
    {
        app = command;
        command = command -> next;
        free(app);
    }
    history -> futureCommands = NULL;
    return;
}

void addNewEventToHistory(t_history *history, t_command *command)
{
    t_command *newHead;

    // only change and delete commands should be saved
    if(command -> type == 'c' || command -> type == 'd')
    {
        newHead = command;
        newHead -> next = history -> pastCommands;
        history -> pastCommands = newHead;
    }
    return;
}

void backToThePast(t_history *history, t_text *text)
{
    t_command *command, *app;
    char **strApp;

    // no commands to do
    if(history -> pastCommands == NULL)
        return;

    command = history -> pastCommands;
    // change command to redo
    if(command -> type == 'c')
    {
        revertChange(command, text);
    }
    // delete command to redo
    else if(command -> type == 'd')
    {
        if(command -> prevData.text != NULL)
            addTextInMiddle(text, command -> prevData, command -> start, command -> end);
    }
    #ifdef DEBUG
    else
    {
        printf("ERROR: try to time travel with a bad command\n");
    }
    #endif

    // command go to other stack, so set next as head of commands stack
    app = history -> pastCommands -> next;
    if(history -> futureCommands == NULL)
    {
        command -> next = NULL;
    }
    else
    {
        command -> next =  history -> futureCommands;
    }
    history -> futureCommands = command;
    // set command as head for the second commands stack
    history -> pastCommands = app;
    return;
}

void backToTheFuture(t_history *history, t_text *text)
{
    t_command *command, *app;

    // no commands to do
    if(history -> futureCommands == NULL)
        return;

    command = history -> futureCommands;
    // change command to redo
    if(command -> type == 'c')
    {
        revertChange(command, text);
    }
    // delete command to redo
    else if(command -> type == 'd')
    {
        deleteCommand(command, text);
    }
    else
    {
        #ifdef DEBUG
            printf("ERROR: try to time travel with a bad command\n");
        #endif
    }

    // command go to other stack, so set next as head of commands stack
    app = history -> futureCommands -> next;
    if(history -> pastCommands == NULL)
    {
        command -> next = NULL;
    }
    else
    {
        command -> next = history -> pastCommands;
    }
    history -> pastCommands = command;
    // set command as head for the second commands stack
    history -> futureCommands = app;
    return;
}

void revertChange(t_command *command, t_text *text)
{
    t_data app;

    // avoid editing of prevData pointer
    app = command -> prevData;
    // restore prev data
    if(app.text == NULL)
    {
        free(text -> lines);
        *text = createText();
    }
    else
    {
        rewriteText(text, app, command -> start, command -> end);
    }

    // swap
    command -> prevData = command -> data;
    command -> data = app;
    return;
}


// ----- TEXT MANAGER -----

t_text createText()
{
    t_text text;
    text.numLines = 0;
    text.lines = malloc(sizeof(char *) * TEXT_BUFFER_SIZE);
    return text;
}

t_data readText(t_text *text, int start, int end)
{
    t_data data;
    int numLinesToRead;

    data.text = NULL;
    data.length = 0;

    // end cannot be greater then numLines
    if(end > text -> numLines)
        end = text -> numLines;

    numLinesToRead = end - start + 1;

    // check if start not exceed numLines, otherwise no data to read
    if(start > text -> numLines)
        return data;

    // check if end not exceed numLines, otherwise end must be decrease to it
    if(end > text -> numLines)
        end = text -> numLines;

    // allocate an array of strings
    // 1,3c -> 3 - 1 + 1 = 3 lines to write
    data.text = malloc(sizeof(char *) * numLinesToRead);

    // read lines
    for(int i = 0; i < numLinesToRead; i++)
    {
        data.text[i] = text -> lines[start + i - 1];
    }

    // set text length
    data.length = numLinesToRead;

    return data;
}

void deleteTextLines(t_text *text, int start, int end)
{
    int numLinesToDelete = end - start + 1;

    // start cannot be greater than text num lines
    if(start > text -> numLines)
        return;

    // cannot delete a num lines lower or equal than 0
    if(numLinesToDelete <= 0)
        return;

    // if end is bigger or equal text num lines
    // than simply decrease text num lines to overwrite them
    if(end >= text -> numLines)
    {
        text -> numLines = text -> numLines - (text -> numLines - start + 1);
        return;
    }

    // start cannot be less or equal than 0
    if(start <= 0)
        start = 1;

    // overwrite lines
    shiftText(text, start, end);

    // 3. save new num lines
    text -> numLines = text -> numLines - numLinesToDelete;
}

void shiftText(t_text *text, int start, int end)
{
    int numLinesToShift;
    int middle = start + (end - start + 1) / 2;
    // check where there are lower data, head or tail
    if(middle < (text -> numLines / 2))
    {
        numLinesToShift = start - 1;
        for(int i = 0; i < numLinesToShift; i++)
        {
            // overwrite lines from start to end
            text -> lines[end - i - 1] = text -> lines[i];
        }
    }
    else
    {
        numLinesToShift = text -> numLines - end;
        for(int i = 0; i < numLinesToShift; i++)
        {
            // overwrite lines from start to end
            text -> lines[start + i - 1] = text -> lines[end + i];
        }
    }
}

void writeText(t_text *text, t_data data, int start, int end)
{
    int numLinesToWrite;
    int numCurrLine;

    // check data
    if(!isDataValidForWrite(text, data, start))
        return;

    // check if this write will append new text
    if(end > text -> numLines)
    {
        // check if exceed allocated memory, otherwise realloc
        if(checkIfExceedBufferSize(text -> numLines, end))
        {
            // reallocate a new area with a more buffer size
            text -> lines = allocateBiggerTextArea(text, end);
        }
        // update text num lines
        text -> numLines = end;
    }

    // write lines
    writeDataToText(text, data, start);
    return;
}

void rewriteText(t_text *text, t_data data, int start, int end)
{
    // check data
    if(!isDataValidForWrite(text, data, start))
        return;

    // check for new lines to add
    // space is already checked for reallocation in write text
    if(end > text -> numLines)
    {
        // save new num lines
        text -> numLines =  text -> numLines + (end - text -> numLines);
    }

    // start cannot be less or equal than 0
    if(start <= 0)
        start = 1;

    // add lines
    writeDataToText(text, data, start);

    return;
}

void addTextInMiddle(t_text *text, t_data data, int start, int end)
{
    int numLinesToAdd = data.length;
    int newLastLine = text -> numLines + numLinesToAdd;
    int numLinesToMove = text -> numLines - start + 1;
    int numTextBuffersAllocated;

    // check data
    if(!isDataValidForWrite(text, data, start))
        return;

    // create space
    createSpaceInMiddleText(text, numLinesToMove, newLastLine);

    // write lines
    writeDataToText(text, data, start);

    // save new num lines
    text -> numLines = newLastLine;
    return;
}

t_boolean isDataValidForWrite(t_text *text, t_data data, int start)
{
    // start cannot be greater than text num lines
    if(start > text -> numLines + 1)
        return false;

    // cannot delete a num lines lower or equal than 0
    if(data.length <= 0)
        return false;

    return true;
}

// checkAndReallocText: check if exceed allocated memory, than realloc
void checkAndReallocText(t_text *text, int newLastLine)
{
    int numTextBuffersAllocated;
    numTextBuffersAllocated = (text -> numLines / TEXT_BUFFER_SIZE) + 1;
    if((newLastLine / TEXT_BUFFER_SIZE  + 1) > numTextBuffersAllocated)
    {
        text -> lines = realloc(text -> lines, sizeof(char *) * numTextBuffersAllocated * TEXT_BUFFER_SIZE);
    }
}

// createSpaceInMiddleText: move text lines to create space
void createSpaceInMiddleText(t_text *text, int numLinesToMove, int newLastLine)
{
    for(int i = 0; i < numLinesToMove; i++)
    {
        // overwrite lines from start to new end
        text -> lines[newLastLine - i - 1] = text -> lines[text -> numLines - i - 1];
    }
}

void writeDataToText(t_text *text, t_data data, int start)
{
    int numCurrLine;
    for(int i = 0; i < data.length; i++)
    {
        numCurrLine = start + i - 1;
        text -> lines[numCurrLine] = data.text[i];
    }
    return;
}

t_boolean checkIfExceedBufferSize(int currNumLines, int newNumLines)
{
    return (newNumLines / TEXT_BUFFER_SIZE) > (currNumLines / TEXT_BUFFER_SIZE + 1);
}

char **allocateBiggerTextArea(t_text *actualText, int newLastLine)
{
    return realloc(actualText -> lines, sizeof(char *) * (newLastLine / TEXT_BUFFER_SIZE + 1) * TEXT_BUFFER_SIZE);
}

// ----- UTILITIES FUNCTIONS -----

int stringSize(char* string)
{
    int count = 0;
    while(string[count] != '\0') count++;
    return count;
}

char* readLine()
{
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

void printLine(char* line)
{
    int i = 0;
    while(line[i] != '\0') {
        putchar(line[i]);
        i++;
    }
    putchar('\n');
}

// ----- MAIN -----

int main()
{
    t_text text = createText();
    t_history history = createHistory();
    t_command *command;

    /*
        Execution process:
        1. read command
        2. execute command
        3. update history
    */

    command = readCommand();

    while(command -> type != 'q')
    {
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