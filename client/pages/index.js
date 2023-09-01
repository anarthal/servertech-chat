import Head from "next/head";
import styles from "@/styles/Home.module.css";
import Header from "@/components/Header";
import { TextField, Button } from "@mui/material";
import { useCallback, useState } from "react";
import { loadUser, saveUser, createUser } from "@/lib/user";
import { useRouter } from "next/navigation";

// The home page
export default function HomePage() {
  const [username, setUserName] = useState("");
  const router = useRouter();

  const onButtonClick = useCallback(() => {
    let user = loadUser();
    if (user) {
      user.username = username;
    } else {
      user = createUser(username);
    }
    saveUser(user);
    router.push("/chat");
  }, [router, username]);

  const onInputChange = useCallback(
    (event) => setUserName(event.target.value),
    [setUserName],
  );

  return (
    <>
      <Head>
        <title>BoostServerTech chat</title>
        <link rel="icon" href="/favicon.ico" />
      </Head>

      <div className="flex flex-col">
        <Header />
        <div className={styles.bodycontainer}>
          <div className="text-center p-12">
            <p className="text-3xl p-3 m-0">Welcome to</p>
            <p className="text-7xl p-3 m-0">💬 BoostServerTech Chat 💬</p>
            <p className="text-xl p-3 m-0">
              A chat app written using the Boost C++ libraries
            </p>
          </div>
          <div className="flex justify-center p-12">
            <div
              className="bg-white rounded-2xl p-7 flex flex-col"
              style={{ minWidth: "50%" }}
            >
              <p className="text-center text-5xl p-3 m-0">Ready to try it?</p>
              <div className="pt-8 pr-5 pl-5 pb-3 flex">
                <TextField
                  variant="standard"
                  required={true}
                  placeholder="Choose a username..."
                  className="pr-4 pl-4 flex-1"
                  value={username}
                  onChange={onInputChange}
                />
                <Button
                  variant="contained"
                  disabled={!username}
                  onClick={onButtonClick}
                >
                  Get started
                </Button>
              </div>
            </div>
          </div>
        </div>
      </div>
    </>
  );
}
