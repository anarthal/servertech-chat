
export type User = {
    id: string
    username: string
}

function genRandomName() {
    const number = Math.floor(Math.random() * 999999);
    return `user-${number}`
}

// Not using a proper ID generation because crypto requires TLS
function genRandomId() {
    return `${Math.floor(Math.random() * 999999)}`;
}

const localStorageKey = 'servertech_user'

export function loadUser(): User | null {
    const jsonUser = localStorage.getItem(localStorageKey)
    if (!jsonUser)
        return null
    try {
        let res = JSON.parse(jsonUser)
        if (typeof res['id'] !== 'string' || typeof res['username'] !== 'string') {
            throw new Error('Stored user has incorrect format')
        }
        return res
    } catch (e) {
        return null
    }
}

export function saveUser(user: User) {
    localStorage.setItem(localStorageKey, JSON.stringify(user))
}

export function createUser(username?: string): User {
    return { id: genRandomId(), username: username || genRandomName() }
}
