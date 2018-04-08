def ci_checkers_targets = [
    ["commit-checker", "openttd/compile-farm-ci:commit-checker"],
]
def ci_builds_targets = [
    ["linux-amd64-clang-3.8", "openttd/compile-farm-ci:linux-amd64-clang-3.8"],
    ["linux-amd64-gcc-6", "openttd/compile-farm-ci:linux-amd64-gcc-6"],
    ["linux-i386-gcc-6", "openttd/compile-farm-ci:linux-i386-gcc-6"],
]

def ci_checkers_stages = ci_checkers_targets.collectEntries {
    ["${it[0]}" : generateCI(it[0], it[1])]
}
def ci_builds_stages = ci_builds_targets.collectEntries {
    ["${it[0]}" : generateCI(it[0], it[1])]
}

def generateCI(display_name, image_name) {
    return {
        dir("${display_name}") {
            unstash "source"

            docker.image("${image_name}").withRun("--volumes-from ${hostname} --workdir " + pwd()) { c->
                sh "docker logs --follow ${c.id}"
                sh "exit `docker wait ${c.id}`"
            }
        }
    }
}

node {
    stage("Checkout") {
        checkout scm

        // Ensure we also have origin/master available
        sh "git fetch --no-tags origin master:refs/remotes/origin/master"

        stash name: "source", useDefaultExcludes: false
    }

    stage("Checkers") {
        parallel ci_checkers_stages
    }

    stage("Builds") {
        parallel ci_builds_stages
    }
}

