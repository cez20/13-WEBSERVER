
#include "HttpResponse.hpp"
// constructor
HttpResponse::HttpResponse(const HttpRequest& clientRequest){
    analyseRequest(clientRequest);
}

//Check what kind of HttpResquest tob build the appropriate response
int HttpResponse::analyseRequest(const HttpRequest& clientRequest){
    if (clientRequest.method != "POST" && clientRequest.method != "GET" && clientRequest.method != "DELETE"){
        this->statusCode = "501";
        this->headers["contentType"] = " text/html";
        this->body=extractFileContent(clientRequest.config.getDocumentRoot() + "/501.html");      
        return (1);
    }
    //check if the  path exist, if not, fill the HttpResponse with the error 404
    if (!fileExist(clientRequest.path)){
        this->statusCode = "404 Not Found";
        this->body=extractFileContent(clientRequest.config.getDocumentRoot() + "/404.html");
        this->headers["contentType"] = "text/html";
        return (1);
    }
    if (clientRequest.isCgi){
        std::string output;
        if(clientRequest.method == "GET"){
            output = executeCgiGet(clientRequest);
        }
        else{
            output = executeCgiPost(clientRequest);
        }
        analyseCgiOutput(output);
        this->statusCode = "200 OK";
        return (0);

    }
    else if (clientRequest.method == "DELETE"){
        return(deleteMethod(clientRequest));
    }  
    else{
        return(responseForStatic(clientRequest));
    }
    return (0);
}

 //this fucntion will write on the socket the response to send to the client
int HttpResponse::writeOnSocket(const int& clientSocket){
    std::string response = "HTTP/1.1 " + statusCode + "\r\n";
    if (statusCode != "HTTP/1.1 204 No Content"){
        for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it) {
            response += it->first + ": " + it->second + "\r\n";
        }
        response += "\r\n" + body;
    }
    std::cout << response << std::endl;
    size_t totalBytesSent =0;
    size_t bytesRemaining = response.length();
    
    while(totalBytesSent < response.length()){
      int bytesSent=send(clientSocket, response.c_str(), response.length(), 0);
       if (bytesSent == -1) {
            std::cerr << "error while sending response to the client" << std::endl;
            return -1;
        }
        totalBytesSent += bytesSent;
        bytesRemaining -= bytesSent;
    }
    
    return (0);
}

bool HttpResponse::fileExist(const std::string& filename) {
    std::ifstream file(filename.c_str());
    return file.good();
}

std::string HttpResponse::executeCgiGet(const HttpRequest& clientRequest) {
    std::string output;
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("Error when creating pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("Error wheile forking");
    }
    else if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        //separate variable from URL
        std::string arguments = clientRequest.queryString;
        std::stringstream ss(arguments);
        std::string environVariables;
        std::vector<std::string> argumentList;
        while (std::getline(ss, environVariables, '&')) {
            argumentList.push_back(environVariables);
        }
        //set args for execve
        std::vector<char*> argvList;
        argvList.push_back(const_cast<char*>("/usr/bin/php"));  // Utilisez le chemin correct vers l'interpréteur PHP
        argvList.push_back(const_cast<char*>("/Users/slord/Desktop/13-WEBSERVER/html/test.php"));
        argvList.push_back(nullptr);
        //set envp for execve
        std::vector<char*> envpList;      
        for (size_t i = 0; i < argumentList.size(); ++i) {
            envpList.push_back(const_cast<char*>(argumentList[i].c_str()));  // Définit la variable d'environnement NAME avec la valeur "John"
        }
        envpList.push_back(nullptr);
        char* const* argv = argvList.data();
        char* const* envp = envpList.data();
        if (execve("/usr/bin/php", argv, envp) == -1) {
            std::cerr << "Error with execve" << std::endl;
        }
    }
    else {
        // Processus parent
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);

        char buffer[5000];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            output += std::string(buffer, bytesRead);
        }
        close(pipefd[0]);

         if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
             throw std::runtime_error("Script returned an error");
         }
        return output;
    }

     return output;
}
std::string HttpResponse::executeCgiPost(const HttpRequest& clientRequest) {
    std::string output;
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        throw std::runtime_error("Error when creating pipe");
    }

    pid_t pid = fork();
    if (pid == -1) {
        throw std::runtime_error("Error wheile forking");
    }
    else if (pid == 0) {
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);
        //separate variable from URL
        std::string arguments = clientRequest.body;
        std::stringstream ss(arguments);
        std::string environVariables;
        std::vector<std::string> argumentList;
        while (std::getline(ss, environVariables, '&')) {
            argumentList.push_back(environVariables);
        }
        //set args for execve
        std::vector<char*> argvList;
        argvList.push_back(const_cast<char*>("/usr/bin/php"));  // Utilisez le chemin correct vers l'interpréteur PHP
        argvList.push_back(const_cast<char*>("/Users/slord/Desktop/13-WEBSERVER/html/test.php"));
        argvList.push_back(nullptr);
        //set envp for execve
        std::vector<char*> envpList;      
        for (size_t i = 0; i < argumentList.size(); ++i) {
            envpList.push_back(const_cast<char*>(argumentList[i].c_str()));
        }
        envpList.push_back(nullptr);
        char* const* argv = argvList.data();
        char* const* envp = envpList.data();
        if (execve("/usr/bin/php", argv, envp) == -1) {
            std::cerr << "Error with execve" << std::endl;
        }
    }
    else {
        // Processus parent
        close(pipefd[1]);

        int status;
        waitpid(pid, &status, 0);

        char buffer[5000];
        ssize_t bytesRead;
        while ((bytesRead = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            output += std::string(buffer, bytesRead);
        }
        close(pipefd[0]);

         if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
             throw std::runtime_error("Script returned an error");
         }
        return output;
    }

     return output;
}

void HttpResponse::analyseCgiOutput(const std::string& output){
        this->body = (output);
        //this->headers["contentDispositon"] = "inline";
        this->headers["contentLength"] = std::to_string(this->body.length());
        return;
    }

int HttpResponse::responseForStatic(const HttpRequest& clientRequest){
   if (clientRequest.toBeDownloaded) {
        std::string filePath = clientRequest.path;  // Chemin du fichier à télécharger
        std::ifstream fileStream(filePath, std::ios::binary);
        std::string fileName = filePath.substr(filePath.find_last_of('/') + 1);
        std::cout << "to be downloaded!!!" << std::endl;
         std::cout << fileName << std::endl;
        // Définir les en-têtes de la réponse
        this->headers["contentDisposition"] = "attachment; filename=\"" + fileName + "\"";
        this->headers["contentType"] = "application/octet-stream"; // Type MIME pour les fichiers téléchargeables
        this->headers["Cache-Control"] = "no-cache";  

        // Lire le contenu du fichier et l'affecter à la réponse
        std::stringstream fileContent;
        fileContent << fileStream.rdbuf();
        this->body = fileContent.str();

        // Définir la longueur du contenu
        this->headers["contentLength"] = std::to_string(this->body.length());
        return 0;
    }
    else{
        this->statusCode = "200 OK";
        this->headers["contentType"] = "text/html";
        this->body = extractFileContent(clientRequest.path);
        this->headers["contentLength"] = std::to_string(this->body.length());
    } 
        return (0);

}


int HttpResponse::deleteMethod(const HttpRequest& clientRequest){
    if (clientRequest.isCgi) {
            std::string output  = executeCgiGet(clientRequest);
            analyseCgiOutput(output);
            return(0);
    }
    this->statusCode = "200 OK";
    this->headers["contentType"] = "text/html";
    this->body = "DELETE request handled successfully.";
    
    return(0);

}

    




