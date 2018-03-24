def ci_targets = [
    ["linux-amd64", "openttd/compile-farm-ci:linux-amd64"],
    ["linux-i386", "openttd/compile-farm-ci:linux-i386"],
]
def ci_stages = ci_targets.collectEntries {
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
        stash name: "source", useDefaultExcludes: false
    }

    stage("CI") {
        parallel ci_stages
    }
}

