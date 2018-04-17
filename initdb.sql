/* XXX db format subject to change */
CREATE TABLE IF NOT EXISTS room (
    /*
     * These things look a little like apr1 hashes but
     * aren't, they contain characters not legal for base64:
     *    $apr1$dF!!_X#W$zUxMycg35omZ3p973Tllm1
     * Just store as text for now.
     */
    id TEXT PRIMARY KEY,
    shortdesc TEXT NOT NULL,
    longdesc TEXT NOT NULL,
    area TEXT,
    indoors BOOLEAN,
    exits TEXT
);

CREATE TABLE IF NOT EXISTS exit (
    direction TEXT,
    source TEXT,
    destination TEXT,
    FOREIGN KEY(source) REFERENCES room(id),
    FOREIGN KEY(destination) REFERENCES room(id),
    PRIMARY KEY(direction, source, destination)
);
