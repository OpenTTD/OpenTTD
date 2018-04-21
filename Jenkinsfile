#!/usr/bin/env groovy

// The stages we run one by one
// Please don't add more than 2 items in a single stage; this hurts performance
def ci_stages = [
    "Checkers": [
        "commit-checker": "openttd/compile-farm-ci:commit-checker",
    ],
    "Compilers": [
        "linux-amd64-gcc-6": "openttd/compile-farm-ci:linux-amd64-gcc-6",
        "linux-amd64-clang-3.8": "openttd/compile-farm-ci:linux-amd64-clang-3.8",
    ],
    "Archs": [
        "linux-i386-gcc-6": "openttd/compile-farm-ci:linux-i386-gcc-6",
    ],
    "OS": [
        "osx-10.9": "openttd/compile-farm-ci:osx-10.9",
    ],
]

def generateStage(targets) {
    return targets.collectEntries{ key, target ->
        ["${key}": generateCI(key, target)]
    }
}

def generateCI(display_name, image_name) {
    return {
        githubNotify context: 'openttd/' + display_name, description: 'This commit is being built', status: 'PENDING'

        try {
            dir("${display_name}") {
                unstash "source"

                docker.image("${image_name}").pull()
                docker.image("${image_name}").withRun("--volumes-from ${hostname} --workdir " + pwd()) { c ->
                    sh "docker logs --follow ${c.id}; exit `docker wait ${c.id}`"
                }
            }

            githubNotify context: 'openttd/' + display_name, description: 'The commit looks good', status: 'SUCCESS'
        }
        catch (error) {
            githubNotify context: 'openttd/' + display_name, description: 'The commit cannot be built', status: 'FAILURE'
            throw error
        }
    }
}

node {
    ansiColor('xterm') {
        stage("Checkout") {
            checkout scm

            // Ensure user.email and user.name is set, otherwise rebase cannot work
            sh "git config user.email 'info@openttd.org'"
            sh "git config user.name 'OpenTTD CI'"

            // Ensure we also have origin/master available
            sh "git fetch --no-tags origin master:refs/remotes/origin/master"

            // Try to rebase to origin/master; if this fails, fail the CI
            sh "git rebase origin/master"

            stash name: "source", useDefaultExcludes: false
        }

        ci_stages.each { ci_stage ->
            stage(ci_stage.key) {
                parallel generateStage(ci_stage.value)
            }
        }
    }
}

