import Head from "@/components/Head";
import styles from "@/styles/Home.module.css";
import Header from "@/components/Header";
import { Button, Link, FormHelperText } from "@mui/material";
import { useCallback, useEffect, useState } from "react";
import { useForm, SubmitHandler } from "react-hook-form";
import useLoading, { DontClearLoading } from "@/hooks/useLoading";
import { login } from "@/lib/api";
import { ErrorId } from "@/lib/apiTypes";
import { useRouter } from "next/router";
import EmailInput from "@/components/EmailInput";
import PasswordInput from "@/components/PasswordInput";
import FormCard from "@/components/FormCard";
import { hasAuth, setHasAuth } from "@/lib/hasAuth";

type Inputs = {
  email: string;
  password: string;
};

const LoginScreen = () => {
  const router = useRouter();

  const {
    register,
    handleSubmit,
    setError,
    formState: { errors },
  } = useForm<Inputs>({ mode: "onTouched" });

  const { loading, launch } = useLoading();
  const onSubmit: SubmitHandler<Inputs> = useCallback(
    async (inputs) => {
      await launch(async () => {
        try {
          const { type } = await login(inputs);
          switch (type) {
            case "ok":
              setHasAuth();
              router.push("/chat");
              return DontClearLoading;
            case ErrorId.LoginFailed:
              setError("root", {
                type: "value",
                message: "Invalid username or password.",
              });
              break;
          }
        } catch {
          setError("root", {
            type: "value",
            message: "There was an unexpected error. Please try again later.",
          });
        }
      });
    },
    [router, launch, setError],
  );

  return (
    <>
      <Head />

      <div className="flex flex-col">
        <Header showArrow={false} />
        <div
          className={`${styles.bodycontainer} p-12 flex flex-col justify-center`}
        >
          <div className="flex justify-center">
            <FormCard title="Log in to BoostServerTech chat">
              <form
                onSubmit={handleSubmit(onSubmit)}
                className="flex-1 flex flex-col"
              >
                <EmailInput
                  register={register}
                  name="email"
                  errorMessage={errors?.email?.message}
                  className="flex-1 pb-2"
                />
                <PasswordInput
                  register={register}
                  name="password"
                  errorMessage={errors?.password?.message}
                  className="flex-1 pb-2 "
                />
                {errors.root && (
                  <FormHelperText error>{errors.root.message}</FormHelperText>
                )}
                <div className="pt-8 flex justify-center">
                  <Button variant="contained" type="submit" disabled={loading}>
                    {loading ? "Logging in..." : "Log me in!"}
                  </Button>
                </div>
                <div className="pt-8 flex justify-center">
                  <p className="p-0 m-0 text-sm">
                    Don&apos;t have an account? Go to{" "}
                    <Link href="/">Create account</Link>
                  </p>
                </div>
              </form>
            </FormCard>
          </div>
        </div>
      </div>
    </>
  );
};

export default function LoginPage() {
  const router = useRouter();

  const [loading, setLoading] = useState(true);

  useEffect(() => {
    if (hasAuth()) {
      router.replace("/chat");
    } else {
      setLoading(false);
    }
  }, [router, setLoading]);

  if (loading) return <></>;

  return <LoginScreen />;
}
