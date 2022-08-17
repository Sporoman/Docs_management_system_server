#ifndef COMMANDS_H
#define COMMANDS_H

namespace Commands
{
    enum Command
    {
        error,
        identity,
        quit,
        userInfo,
        getDocs,
        getFavDocs,
        addFavoriteDoc,
        deleteFavoriteDoc,
        sendDocInfo,
        sendDocFullInfo,
        sendDocToClient,
        sendDocToServer,
        getRolesAndLevels,
        addNewUser,
        searchUserForEdit,
        searchUserForShow,
        editUser,
        editUserInfo,
        setUserStatus,
        editDocInfo,
        deleteDoc,
        getStatistics
    };

    const char* getNameCommand(Command commandNumber);
}

#endif // COMMANDS_H
